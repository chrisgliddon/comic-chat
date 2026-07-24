/**
 * Port of `vector2d.cpp` — 2D vector / point math.
 *
 * Source: `v1.0-pre-modern/vector2d.cpp` (reference), cross-checked against
 * `v2.5-beta-1-modern/vector2d.cpp` (the golden oracle tree). The two trees
 * differ only in two cosmetic lines:
 *   - v1.0-pre uses `fabs(vec.x)`; v2.5 uses `abs(vec.x)` (same semantics on double).
 *   - v2.5 adds `(double)` casts on `atan2(vec.y, vec.x)`.
 * We match the **v2.5** forms since the oracle goldens come from that tree;
 * numerically the casts are no-ops in JS (numbers are doubles already).
 *
 * The integer POINT overloads (`point_scalmult`, `dpoint_to_point`) use
 * `INT_CAST` / `ROUND` from core/numeric — see RULEBOOK §ROUND.
 */

import { ABS, INT_CAST, PI, SMALLNUMBER, TWO_PI } from "../core/numeric.js";
import type { DPOINT, POINT } from "../core/types.js";

// ===========================================================================
// DPOINT overloads (vector2d.cpp:12-78)
// ===========================================================================

export function point_sub_d(pt1: DPOINT, pt2: DPOINT): DPOINT {
  return { x: pt1.x - pt2.x, y: pt1.y - pt2.y };
}

export function point_add_d(pt1: DPOINT, pt2: DPOINT): DPOINT {
  return { x: pt1.x + pt2.x, y: pt1.y + pt2.y };
}

// C signature is `point_scalmult(double scalar, DPOINT pt)` — scalar first.
// We expose separate _d / _i names instead of TS overloads so the call site
// is unambiguous (the rulebook forbids overload resolution surprises).
export function point_scalmult_d(scalar: number, pt: DPOINT): DPOINT {
  return { x: pt.x * scalar, y: pt.y * scalar };
}

export function point_dot_d(pt1: DPOINT, pt2: DPOINT): number {
  return pt1.x * pt2.x + pt1.y * pt2.y;
}

export function point_dist_d(pt1: DPOINT, pt2: DPOINT): number {
  const diffx = pt1.x - pt2.x;
  const diffy = pt1.y - pt2.y;
  return Math.sqrt(diffx * diffx + diffy * diffy);
}

export function point_distsq_d(pt1: DPOINT, pt2: DPOINT): number {
  const diffx = pt1.x - pt2.x;
  const diffy = pt1.y - pt2.y;
  return diffx * diffx + diffy * diffy;
}

export function point_magn_d(pt: DPOINT): number {
  return Math.sqrt(pt.x * pt.x + pt.y * pt.y);
}

export function point_norm_d(pt: DPOINT): DPOINT {
  const magn = point_magn_d(pt);
  if (magn < SMALLNUMBER) {
    // C: TRACE + zero. TRACE is a no-op in the port (RULEBOOK §Diagnostics).
    return { x: 0.0, y: 0.0 };
  }
  return point_scalmult_d(1.0 / magn, pt);
}

// v2.5 form: `abs` + `(double)` casts on atan2 args. Numerically identical to
// v1.0-pre's `fabs` form in JS; matching the oracle tree for provenance.
export function vector_to_angle_d(vec: DPOINT): number {
  if (ABS(vec.x) < SMALLNUMBER && ABS(vec.y) < SMALLNUMBER) {
    return 0.0;
  }
  return Math.atan2(vec.y, vec.x);
}

// ===========================================================================
// POINT (integer) overloads (vector2d.cpp:82-167)
// ===========================================================================

export function point_sub_i(pt1: POINT, pt2: POINT): POINT {
  return { x: pt1.x - pt2.x, y: pt1.y - pt2.y };
}

export function point_add_i(pt1: POINT, pt2: POINT): POINT {
  return { x: pt1.x + pt2.x, y: pt1.y + pt2.y };
}

// vector2d.cpp:100 — `pt.x = (int)(pt.x * scalar)` (truncation toward zero).
export function point_scalmult_i(scalar: number, pt: POINT): POINT {
  return { x: INT_CAST(pt.x * scalar), y: INT_CAST(pt.y * scalar) };
}

export function point_dot_i(pt1: POINT, pt2: POINT): number {
  return pt1.x * pt2.x + pt1.y * pt2.y;
}

export function point_dist_i(pt1: POINT, pt2: POINT): number {
  const diffx = pt1.x - pt2.x;
  const diffy = pt1.y - pt2.y;
  return Math.sqrt(diffx * diffx + diffy * diffy);
}

export function point_distsq_i(pt1: POINT, pt2: POINT): number {
  const diffx = pt1.x - pt2.x;
  const diffy = pt1.y - pt2.y;
  return diffx * diffx + diffy * diffy;
}

// manhattan_dist (vector2d.cpp:124) — C `abs` on int.
export function manhattan_dist(pt1: POINT, pt2: POINT): number {
  return Math.abs(pt1.x - pt2.x) + Math.abs(pt1.y - pt2.y);
}

export function point_magn_i(pt: POINT): number {
  // v2.5: `sqrt((double)(pt.x * pt.x + pt.y * pt.y))` — cast is a no-op in JS.
  return Math.sqrt(pt.x * pt.x + pt.y * pt.y);
}

export function point_norm_i(pt: POINT): POINT {
  const magn = point_magn_i(pt);
  if (magn < SMALLNUMBER) {
    return { x: 0, y: 0 };
  }
  return point_scalmult_i(1.0 / magn, pt);
}

export function vector_to_angle_i(vec: POINT): number {
  if (ABS(vec.x) < SMALLNUMBER && ABS(vec.y) < SMALLNUMBER) {
    return 0.0;
  }
  return Math.atan2(vec.y, vec.x);
}

// ===========================================================================
// Angle routines (vector2d.cpp:169-203)
// ===========================================================================

export function degrees_to_rads(degrees: number): number {
  return degrees * (PI / 180.0);
}

export function angle_to_vector(angle: number): DPOINT {
  return { x: Math.cos(angle), y: Math.sin(angle) };
}

// value_to_angle — normalize to (-PI, PI].
// C: `temp = (temp - (int)temp) * 2*PI`. `(int)temp` is truncation toward zero,
// which for negative temp keeps the fractional part negative — matching JS
// `Math.trunc`. The parenthesization `(temp - (int)temp) * 2*PI` in C evaluates
// left-to-right: ((temp - (int)temp) * 2) * PI. We replicate exactly.
export function value_to_angle(value: number): number {
  if (value > -PI && value <= PI) return value;
  let temp = value / TWO_PI;
  temp = (temp - Math.trunc(temp)) * 2 * PI;
  if (temp > PI) return temp - TWO_PI;
  else if (temp <= -PI) return temp + TWO_PI;
  else return temp;
}

export function add_angles(angle1: number, angle2: number): number {
  return value_to_angle(angle1 + angle2);
}

export function subtract_angles(angle1: number, angle2: number): number {
  return value_to_angle(angle1 - angle2);
}

export function angle_between_vecs(vec1: DPOINT, vec2: DPOINT): number {
  return subtract_angles(vector_to_angle_d(vec2), vector_to_angle_d(vec1));
}

// dpoint_to_point is re-exported here for parity with the C file's location,
// even though the implementation lives in core/types (it needs ROUND which is
// in core/numeric — types.ts imports it). Re-export keeps the public surface
// matching the C header.
export { dpoint_to_point, point_to_dpoint } from "../core/types.js";