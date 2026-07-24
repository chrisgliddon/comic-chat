/**
 * Port of `spline.cpp` + `splinutl.cpp` — Cardinal/Beta spline -> Bézier
 * conversion, plus the Bézier utility routines (split, flatten, nearest-point,
 * walk-horizontal-distance).
 *
 * Sources:
 *   - `v1.0-pre-modern/spline.cpp`     (reference, readable)
 *   - `v1.0-pre-modern/splinutl.cpp`   (reference, byte-identical to oracle)
 *   - `v2.5-beta-1-modern/spline.cpp`  (golden oracle tree — only the
 *     copyright header differs from v1.0-pre for the math paths exercised
 *     here; CCardinal/CBeta ctor + SetMatrix + ComputeBezpts + GetKnot +
 *     CvertsToCubic + CubicToBezier are line-identical.)
 *
 * This is the Phase-3 shakedown target: the balloon outline is a CBeta spline
 * with the frozen defaults `tension=5.0, bias=1.0` (spline.cpp:68-69), and the
 * oracle corpus dumps the cps[] + bezpts[] for every balloon. The corpus-001
 * first balloon gives a real golden (nCps=22, closed, BezierCount=67).
 *
 * Design notes (RULEBOOK §Geometry, §Value-Types):
 *   - The C `CSpline`/`CCardinal`/`CBeta` classes are value-type-ish (no MFC
 *     inheritance; `CMapWordToPtr`/`CMapStringToPtr` only cache the matrix).
 *     We port them as TS classes with the same field names. The matrix cache
 *     is dropped — it's a pure memoization that doesn't affect output, and the
 *     TS port re-derives the 4x4 matrix per instance (the engine only makes a
 *     handful of splines per panel).
 *   - `POINT` arithmetic in CvertsToCubic/CubicToBezier uses `ROUND` (the
 *     load-bearing macro) — pinned in core/numeric. Do not substitute
 *     Math.round.
 *   - `ComputeBezpts` allocates exactly `BezierCount()` entries and writes
 *     bezpts[0] then 3 per cubic segment. The harness bounds its dump loop
 *     the same way (oracleharness.cpp:297) — the port's array length is the
 *     same invariant.
 *   - `GetKnot` for closed splines has the exact wraparound from spline.cpp:236.
 *   - The `sprintf("%f*%f", tension, bias)` cache key in CBeta::SetMatrix is
 *     format-dependent, but since we drop the cache the key is moot; we keep a
 *     comment for provenance.
 */

import { ROUND } from "../core/numeric.js";
import type { BEZIER, BOUNDBOX, DPOINT, POINT } from "../core/types.js";
import { dpoint_to_point, point } from "../core/types.js";
import {
  point_add_d,
  point_dist_d,
  point_scalmult_d,
  point_sub_d,
} from "./vector2d.js";

// ---------------------------------------------------------------------------
// Matrix type (spline.h:4) — 4x4 double.
// ---------------------------------------------------------------------------
type MATRIX = number[][]; // [4][4]

// ---------------------------------------------------------------------------
// DASHINFO / CSeg / CLine / CArc / CTraj are GDI-trajectory types (traj.h).
// They're not needed for the Phase-3 math shakedown (Draw/Dash call into a
// CDC*). We stub CSpline's GDI surface here and port the full CTraj in Phase 4
// alongside balloon.cpp (its only consumer). The pure-math methods (ComputeBezpts,
// GetKnot, ClosestPoint, WalkHorizontalDistance) are fully ported below.
// ---------------------------------------------------------------------------

// ===========================================================================
// splinutl.cpp — pure Bézier utilities (no GDI)
// ===========================================================================

// split_bezier (splinutl.cpp:23) — de Casteljau split at t=0.5.
export function split_bezier(b: BEZIER, left: BEZIER, right: BEZIER): void {
  const t = point_scalmult_d(0.5, point_add_d(b.p1, b.p2));
  left.p0 = b.p0;
  left.p1 = point_scalmult_d(0.5, point_add_d(b.p0, b.p1));
  left.p2 = point_scalmult_d(0.5, point_add_d(left.p1, t));
  right.p3 = b.p3;
  right.p2 = point_scalmult_d(0.5, point_add_d(b.p2, b.p3));
  right.p1 = point_scalmult_d(0.5, point_add_d(t, right.p2));
  // left.p3 == right.p0 == midpoint of (left.p2, right.p1)
  const mid = point_scalmult_d(0.5, point_add_d(left.p2, right.p1));
  left.p3 = mid;
  right.p0 = mid;
}

// inside_bbox_tol — DPOINT/BOUNDBOX overload (splinutl.cpp:49). Distinct from
// the integer bbox.inside_bbox_tol; kept here next to its only caller.
export function inside_bbox_tol_d(pt: DPOINT, bbox: BOUNDBOX, tol: number): boolean {
  if (
    pt.x + tol < bbox.xmin ||
    pt.x - tol > bbox.xmax ||
    pt.y + tol < bbox.ymin ||
    pt.y - tol > bbox.ymax
  )
    return false;
  return true;
}

// flat_bezier (splinutl.cpp:61) — epsilon = 1.0 (module-global in C).
const epsilon = 1.0;
export function flat_bezier(b: BEZIER): boolean {
  const bbox: BOUNDBOX = {
    xmin: Math.min(b.p0.x, b.p3.x),
    xmax: Math.max(b.p0.x, b.p3.x),
    ymin: Math.min(b.p0.y, b.p3.y),
    ymax: Math.max(b.p0.y, b.p3.y),
  };
  if (!inside_bbox_tol_d(b.p1, bbox, 0.5 * epsilon) ||
      !inside_bbox_tol_d(b.p2, bbox, 0.5 * epsilon))
    return false;

  const d1 = point_sub_d(b.p1, b.p0);
  const d2 = point_sub_d(b.p2, b.p0);
  const d = point_sub_d(b.p3, b.p0);
  const dx = Math.abs(d.x);
  const dy = Math.abs(d.y);
  if (dx + dy < epsilon) return true;
  if (dy < dx) {
    const dydx = d.y / d.x;
    return (
      Math.abs(d2.y - d2.x * dydx) < epsilon &&
      Math.abs(d1.y - d1.x * dydx) < epsilon
    );
  } else {
    const dxdy = d.x / d.y;
    return (
      Math.abs(d2.x - d2.y * dxdy) < epsilon &&
      Math.abs(d1.x - d1.y * dxdy) < epsilon
    );
  }
}

// Callback shapes for subdivide/flatten/walk_path. The C code uses raw
// function pointers + void* arg; we type them as generic callbacks carrying
// their own arg via closure. Returning true terminates the walk (same as C).
type DPointCallback = (pt: DPOINT) => boolean;

// subdivide (splinutl.cpp:95) — walk a Bézier calling `proc` roughly `delta`
// apart on the flattened polyline. Returns when `proc` returns true.
export function subdivide(bezier: BEZIER, proc: DPointCallback, delta: number): boolean {
  if (flat_bezier(bezier)) {
    const length = point_dist_d(bezier.p0, bezier.p3);
    if (length > 1e-24 /* SMALLNUMBER */) {
      const step = delta / length;
      for (let alpha = 0.0; alpha <= 1.0; alpha += step) {
        const pt = point_add_d(
          point_scalmult_d(alpha, bezier.p3),
          point_scalmult_d(1.0 - alpha, bezier.p0),
        );
        if (proc(pt)) return true;
      }
    }
    return proc(bezier.p3);
  } else {
    const left: BEZIER = { p0: { x: 0, y: 0 }, p1: { x: 0, y: 0 }, p2: { x: 0, y: 0 }, p3: { x: 0, y: 0 } };
    const right: BEZIER = { p0: { x: 0, y: 0 }, p1: { x: 0, y: 0 }, p2: { x: 0, y: 0 }, p3: { x: 0, y: 0 } };
    split_bezier(bezier, left, right);
    if (subdivide(left, proc, delta) || subdivide(right, proc, delta)) return true;
    return false;
  }
}

// walk_path (splinutl.cpp:127) — walk an array of Béziers.
export function walk_path(beziers: BEZIER[], proc: DPointCallback, delta = epsilon): boolean {
  for (let i = 0; i < beziers.length; i++) {
    if (subdivide(beziers[i], proc, delta)) return true;
  }
  return false;
}

// cb_on_line (splinutl.cpp:138) — TOL = 2.0.
const TOL = 2.0;
export function cb_on_line(pt: DPOINT, testpt: DPOINT): boolean {
  return Math.abs(pt.x - testpt.x) + Math.abs(pt.y - testpt.y) <= TOL;
}

// nearinfo (splinutl.cpp:145) — the C struct carried given_pt + found_pt +
// dist. In TS we use a class so the callbacks can close over it.
class NearInfo {
  dist: number;
  given_pt: DPOINT;
  found_pt: DPOINT;
  constructor(given_pt: DPOINT) {
    this.given_pt = given_pt;
    this.dist = 1e24; // LARGENUMBER
    this.found_pt = { x: 0, y: 0 };
  }
}

// cb_nearest (splinutl.cpp:209) — manhattan-distance nearest point so far.
function cb_nearest(arg: NearInfo): DPointCallback {
  return (pt: DPOINT): boolean => {
    const thisdist = Math.abs(pt.x - arg.given_pt.x) + Math.abs(pt.y - arg.given_pt.y);
    if (thisdist < arg.dist) {
      arg.dist = thisdist;
      arg.found_pt = pt;
    }
    return false;
  };
}

// spline_nearest_point (splinutl.cpp:150)
export function spline_nearest_point(
  beziers: BEZIER[],
  given_pt: DPOINT,
): { dist: number; found_pt: DPOINT } {
  const arg = new NearInfo(given_pt);
  walk_path(beziers, cb_nearest(arg), epsilon);
  return { dist: arg.dist, found_pt: arg.found_pt };
}

// bezier_nearest_point (splinutl.cpp:167) — double-array overload.
export function bezier_nearest_point(
  bezpts: number[][], // [4][2]
  given_x: number,
  given_y: number,
): { dist: number; found_x: number; found_y: number } {
  const b: BEZIER = {
    p0: { x: bezpts[0][0], y: bezpts[0][1] },
    p1: { x: bezpts[1][0], y: bezpts[1][1] },
    p2: { x: bezpts[2][0], y: bezpts[2][1] },
    p3: { x: bezpts[3][0], y: bezpts[3][1] },
  };
  const given_pt: DPOINT = { x: given_x, y: given_y };
  const { dist, found_pt } = spline_nearest_point([b], given_pt);
  return { dist, found_x: found_pt.x, found_y: found_pt.y };
}

// int_bezier_nearest_point (splinutl.cpp:187) — integer overload. C truncates
// (not rounds) found.x/found.y via `(int)` cast; dist via `(int)`. We match.
export function int_bezier_nearest_point(
  bezpts: POINT[],
  given: POINT,
): { dist: number; found: POINT } {
  const b: BEZIER = {
    p0: { x: bezpts[0].x, y: bezpts[0].y },
    p1: { x: bezpts[1].x, y: bezpts[1].y },
    p2: { x: bezpts[2].x, y: bezpts[2].y },
    p3: { x: bezpts[3].x, y: bezpts[3].y },
  };
  const given_dpoint: DPOINT = { x: given.x, y: given.y };
  const { dist, found_pt } = spline_nearest_point([b], given_dpoint);
  // C: `(int)d_dist` then `(int)found_dpoint.x` — truncation toward zero.
  return {
    dist: Math.trunc(dist),
    found: { x: Math.trunc(found_pt.x), y: Math.trunc(found_pt.y) },
  };
}

// flatten (splinutl.cpp:229) — like subdivide but only calls proc at segment
// endpoints (not intermediate samples).
export function flatten(bezier: BEZIER, proc: DPointCallback): boolean {
  if (flat_bezier(bezier)) {
    return proc(bezier.p3);
  } else {
    const left: BEZIER = { p0: { x: 0, y: 0 }, p1: { x: 0, y: 0 }, p2: { x: 0, y: 0 }, p3: { x: 0, y: 0 } };
    const right: BEZIER = { p0: { x: 0, y: 0 }, p1: { x: 0, y: 0 }, p2: { x: 0, y: 0 }, p3: { x: 0, y: 0 } };
    split_bezier(bezier, left, right);
    if (flatten(left, proc) || flatten(right, proc)) return true;
    return false;
  }
}

// int_bezier_flatten (splinutl.cpp:247)
export function int_bezier_flatten(bezpts: POINT[], proc: DPointCallback): void {
  const b: BEZIER = {
    p0: { x: bezpts[0].x, y: bezpts[0].y },
    p1: { x: bezpts[1].x, y: bezpts[1].y },
    p2: { x: bezpts[2].x, y: bezpts[2].y },
    p3: { x: bezpts[3].x, y: bezpts[3].y },
  };
  flatten(b, proc);
}

// cb_beyond_deltaX (splinutl.cpp:291) — used by walk_horizontal_dist.
// found_pt.x holds best X so far (starts < any spline value); given_pt.x is goalX.
class WalkHorizArg {
  found_pt: DPOINT;
  given_pt: DPOINT; // given_pt.x = goalX
  constructor(goalX: number) {
    this.found_pt = { x: -1000000, y: 0 };
    this.given_pt = { x: goalX, y: 0 };
  }
}
function cb_beyond_deltaX(arg: WalkHorizArg): DPointCallback {
  return (pt: DPOINT): boolean => {
    if (pt.x > arg.found_pt.x) arg.found_pt = pt;
    return pt.x >= arg.given_pt.x;
  };
}

// walk_horizontal_dist (splinutl.cpp:262)
export function walk_horizontal_dist(
  bezpts: POINT[],
  goalX: number,
): { found: boolean; furthest: POINT } {
  const b: BEZIER = {
    p0: { x: bezpts[0].x, y: bezpts[0].y },
    p1: { x: bezpts[1].x, y: bezpts[1].y },
    p2: { x: bezpts[2].x, y: bezpts[2].y },
    p3: { x: bezpts[3].x, y: bezpts[3].y },
  };
  const arg = new WalkHorizArg(goalX);
  const found = walk_path([b], cb_beyond_deltaX(arg));
  return { found, furthest: dpoint_to_point(arg.found_pt) };
}

// ===========================================================================
// spline.cpp — CSpline / CCardinal / CBeta
// ===========================================================================

// Abstract base mirroring CSpline's pure-math surface. GDI methods (Draw/Dash)
// are deferred to Phase 4 (balloon port).
export abstract class CSpline {
  closed: boolean;
  matrix: MATRIX | null = null;
  bezpts: POINT[] | null = null;
  nCps: number;
  cps: POINT[];

  constructor(cpArray: POINT[], n: number, isClosed = false) {
    // C: ASSERT(n >= 2). Port: throw to surface bad inputs (RULEBOOK §Diagnostics).
    if (n < 2) throw new Error(`CSpline: n >= 2 required (got ${n})`);
    this.nCps = n;
    this.cps = cpArray.slice(0, n); // copy, like spline.cpp:17
    this.bezpts = null;
    this.closed = isClosed;
  }

  abstract GetDups(): number;
  abstract KnotCount(): number;
  // Clone() and the GDI Draw/Dash are Phase 4.

  BezierCount(): number {
    return 3 * this.KnotCount() - 8;
  }

  GetKnot(index: number): POINT {
    if (this.closed) {
      // spline.cpp:237 — exact wraparound, no modulo (the C comment says
      // "mod arith would be slower").
      if (index === 0) return this.cps[this.nCps - 1];
      else if (index === this.nCps + 1) return this.cps[0];
      else if (index === this.nCps + 2) return this.cps[1];
      else return this.cps[index - 1];
    } else {
      const dups = this.GetDups();
      if (index < dups) return this.cps[0];
      else if (index >= this.nCps + dups - 2) return this.cps[this.nCps - 1];
      else return this.cps[index - dups + 1];
    }
  }

  // CvertsToCubic (spline.cpp:209) — matrix * 4 knots -> cubic coeffs.
  // Each coeff is ROUND(matrix-row . knots) — ROUND is load-bearing.
  CvertsToCubic(
    k0: POINT, k1: POINT, k2: POINT, k3: POINT,
  ): { c0: POINT; c1: POINT; c2: POINT; c3: POINT } {
    const m = this.matrix!;
    // C lays out: c3 = row0, c2 = row1, c1 = row2, c0 = row3. Keep that order.
    const c3 = point(
      ROUND(m[0][0] * k0.x + m[0][1] * k1.x + m[0][2] * k2.x + m[0][3] * k3.x),
      ROUND(m[0][0] * k0.y + m[0][1] * k1.y + m[0][2] * k2.y + m[0][3] * k3.y),
    );
    const c2 = point(
      ROUND(m[1][0] * k0.x + m[1][1] * k1.x + m[1][2] * k2.x + m[1][3] * k3.x),
      ROUND(m[1][0] * k0.y + m[1][1] * k1.y + m[1][2] * k2.y + m[1][3] * k3.y),
    );
    const c1 = point(
      ROUND(m[2][0] * k0.x + m[2][1] * k1.x + m[2][2] * k2.x + m[2][3] * k3.x),
      ROUND(m[2][0] * k0.y + m[2][1] * k1.y + m[2][2] * k2.y + m[2][3] * k3.y),
    );
    const c0 = point(
      ROUND(m[3][0] * k0.x + m[3][1] * k1.x + m[3][2] * k2.x + m[3][3] * k3.x),
      ROUND(m[3][0] * k0.y + m[3][1] * k1.y + m[3][2] * k2.y + m[3][3] * k3.y),
    );
    return { c0, c1, c2, c3 };
  }

  // CubicToBezier (spline.cpp:221) — cubic coeffs -> Bézier control points.
  // b1/b2 use ROUND((1.0/3.0) * ...) — ROUND is load-bearing.
  CubicToBezier(
    c0: POINT, c1: POINT, c2: POINT, c3: POINT,
  ): { b0: POINT; b1: POINT; b2: POINT; b3: POINT } {
    const b0 = point(c0.x, c0.y);
    const b1 = point(
      c0.x + ROUND((1.0 / 3.0) * c1.x),
      c0.y + ROUND((1.0 / 3.0) * c1.y),
    );
    const b2 = point(
      b1.x + ROUND((1.0 / 3.0) * (c1.x + c2.x)),
      b1.y + ROUND((1.0 / 3.0) * (c1.y + c2.y)),
    );
    const b3 = point(c0.x + c1.x + c2.x + c3.x, c0.y + c1.y + c2.y + c3.y);
    return { b0, b1, b2, b3 };
  }

  // ComputeBezpts (spline.cpp:172) — the core. Allocates BezierCount() entries
  // and fills bezpts[0] then 3 per cubic segment. The loop structure mirrors
  // the C `for (int i = 0; 1; i++)` with the `i + 4 == nKnots` exit.
  ComputeBezpts(): void {
    const nKnots = this.KnotCount();
    if (nKnots < 4) throw new Error(`CSpline.ComputeBezpts: nKnots >= 4 required (got ${nKnots})`);
    if (!this.bezpts) this.bezpts = new Array(this.BezierCount());

    let bezIndex = 1;
    let knot0 = this.GetKnot(0);
    let knot1 = this.GetKnot(1);
    let knot2 = this.GetKnot(2);
    let knot3 = this.GetKnot(3);
    for (let i = 0; ; i++) {
      const { c0, c1, c2, c3 } = this.CvertsToCubic(knot0, knot1, knot2, knot3);
      const { b0, b1, b2, b3 } = this.CubicToBezier(c0, c1, c2, c3);
      if (i === 0) this.bezpts[0] = b0;
      this.bezpts[bezIndex] = b1;
      this.bezpts[bezIndex + 1] = b2;
      this.bezpts[bezIndex + 2] = b3;
      if (i + 4 === nKnots) return;
      bezIndex += 3;
      knot0 = knot1;
      knot1 = knot2;
      knot2 = knot3;
      knot3 = this.GetKnot(i + 4);
    }
  }

  // ClosestPoint (spline.cpp:254) — searches each Bézier segment for the
  // nearest point to `toPt`, returns the closest and the knot index.
  ClosestPoint(toPt: POINT): { pos: POINT; knotIndex: number } {
    let minDist = 10000000;
    const bezCount = this.BezierCount();
    let minPos = point(0, 0);
    let knotIndex = 0;
    for (let i = 0; i < bezCount - 1; i += 3) {
      const bezSeg = this.bezpts!.slice(i, i + 4);
      const { dist, found } = int_bezier_nearest_point(bezSeg, toPt);
      if (dist < minDist) {
        minDist = dist;
        minPos = found;
        knotIndex = Math.trunc(i / 3) + 2;
      }
    }
    return { pos: minPos, knotIndex };
  }

  // WalkHorizontalDistance (spline.cpp:272) — walk bezpts looking for a point
  // reaching goalX. The C index arithmetic is preserved verbatim including the
  // "rethink this" comments' behavior (the wrap `index+3 > bezCount-1 -> 0`).
  WalkHorizontalDistance(
    fromPt: POINT, fromKnotIndex: number, goalX: number,
  ): { furthest: POINT; foundKnotIndex: number } {
    const bezCount = this.BezierCount();
    let foundKnotIndex = -1;
    let index = (fromKnotIndex - 2) * 3;

    let furthest = point(0, 0);
    let lastFurthest = point(-100000, -100000);

    for (let i = 0; i < bezCount - 1; i += 3) {
      if (index + 3 > bezCount - 1) index = 0;
      const bezSeg = this.bezpts!.slice(index, index + 4);
      const { found, furthest: f } = walk_horizontal_dist(bezSeg, goalX);
      if (found) {
        foundKnotIndex = Math.trunc(index / 3) + 2;
        return { furthest: f, foundKnotIndex };
      }
      if (f.x > lastFurthest.x) {
        foundKnotIndex = Math.trunc(index / 3) + 2;
        lastFurthest = f;
      }
      index += 3;
    }
    // C: ASSERT(foundKnotIndex > 0). Port: surface as error (RULEBOOK §Diagnostics).
    if (foundKnotIndex <= 0) throw new Error("CSpline.WalkHorizontalDistance: no knot found");
    void fromPt; // C passes fromPt but the body doesn't read it — keep signature parity.
    return { furthest: lastFurthest, foundKnotIndex };
  }

  SegLo(): POINT {
    return this.bezpts![0];
  }
}

// ---------------------------------------------------------------------------
// CCardinal (spline.cpp:47-66 + SetMatrix :96)
// ---------------------------------------------------------------------------
export class CCardinal extends CSpline {
  static defaultTension = 0.4;
  tension: number;

  constructor(cpArray: POINT[], n: number, isClosed = false) {
    super(cpArray, n, isClosed);
    this.tension = CCardinal.defaultTension;
    this.SetMatrix(this.tension);
    this.ComputeBezpts(); // C calls this in the ctor (spline.cpp:53)
  }

  GetDups(): number { return 2; }
  KnotCount(): number { return this.closed ? this.nCps + 3 : this.nCps + 2; }

  SetMatrix(tension: number): void {
    // spline.cpp:96 — cardinalMatrixMap memoization dropped (output-equivalent).
    const m: MATRIX = [
      [0, 0, 0, 0],
      [0, 0, 0, 0],
      [0, 0, 0, 0],
      [0, 0, 0, 0],
    ];
    m[0][1] = 2.0 - tension;
    m[0][2] = tension - 2.0;
    m[1][0] = 2.0 * tension;
    m[1][1] = tension - 3.0;
    m[1][2] = 3.0 - 2.0 * tension;
    m[3][1] = 1.0;
    m[0][3] = tension;
    m[2][2] = tension;
    m[0][0] = -tension;
    m[1][3] = -tension;
    m[2][0] = -tension;
    m[2][1] = 0.0;
    m[2][3] = 0.0;
    m[3][0] = 0.0;
    m[3][2] = 0.0;
    m[3][3] = 0.0;
    this.matrix = m;
  }
}

// ---------------------------------------------------------------------------
// CBeta (spline.cpp:68-90 + SetMatrix :115)
// ---------------------------------------------------------------------------
export class CBeta extends CSpline {
  static defaultTension = 5.0;
  static defaultBias = 1.0;
  tension: number;
  bias: number;

  constructor(cpArray: POINT[], n: number, isClosed = false) {
    super(cpArray, n, isClosed);
    this.tension = CBeta.defaultTension;
    this.bias = CBeta.defaultBias;
    this.SetMatrix(this.tension, this.bias);
    this.ComputeBezpts(); // spline.cpp:76
  }

  GetDups(): number { return 3; }
  KnotCount(): number { return this.closed ? this.nCps + 3 : this.nCps + 4; }

  SetMatrix(tension: number, bias: number): void {
    // spline.cpp:115 — the C cache key is `sprintf(key, "%f*%f", tension, bias)`.
    // We drop the cache (output-equivalent); the matrix derivation below is
    // line-for-line from the C, including the final `*= d` scaling loop.
    const m: MATRIX = [
      [0, 0, 0, 0],
      [0, 0, 0, 0],
      [0, 0, 0, 0],
      [0, 0, 0, 0],
    ];
    const b2 = bias * bias;
    const b3 = bias * b2;
    const d = 1.0 / (tension + 2.0 * b3 + 4.0 * (b2 + bias) + 2.0);

    m[0][0] = -2.0 * b3;
    m[0][1] = 2.0 * (tension + b3 + b2 + bias);
    m[0][2] = -2.0 * (tension + b2 + bias + 1.0);
    m[1][0] = 6.0 * b3;
    m[1][1] = -3.0 * (tension + 2.0 * (b3 + b2));
    m[1][2] = 3.0 * (tension + 2.0 * b2);
    m[2][0] = -6.0 * b3;
    m[2][1] = 6.0 * (b3 - bias);
    m[2][2] = 6.0 * bias;
    m[3][0] = 2.0 * b3;
    m[3][1] = tension + 4.0 * (b2 + bias);
    m[0][3] = 2.0;
    m[3][2] = 2.0;
    m[1][3] = 0.0;
    m[2][3] = 0.0;
    m[3][3] = 0.0;

    for (let i = 0; i < 4; i++)
      for (let j = 0; j < 4; j++) m[i][j] *= d;

    this.matrix = m;
  }
}

// DestroySplineMatrixCaches (spline.cpp:150) — no-op in the port (cache dropped).
export function DestroySplineMatrixCaches(): void { /* no-op */ }