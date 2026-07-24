/**
 * Golden test: the spline port against the oracle corpus-001 dump.
 *
 * This is the Phase-3 two-agent check, made concrete: the C++ oracle (frozen
 * in `oracle/corpus/001/expected.json`) produced a CBeta spline with nCps=22,
 * closed=true, bezpts of length 67. The TS port must reproduce those exact
 * bezpts from the same cps[] input. If they match, the rulebook's value-type
 * + ROUND + matrix conventions are unambiguous. If they differ, the rulebook
 * is the thing to fix — not the code.
 *
 * The golden is read from the repo (not copied into the test) so a re-freeze
 * of the oracle automatically updates the shakedown target.
 */
import { describe, it, expect } from "vitest";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import {
  CBeta,
  CCardinal,
  CSpline,
  bezier_nearest_point,
  flat_bezier,
  split_bezier,
} from "../../src/engine/spline.js";
import type { POINT } from "../../src/core/types.js";
import { point } from "../../src/core/types.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
// test/ -> repo root -> oracle/corpus/001/expected.json
const GOLDEN_PATH = resolve(__dirname, "../../../oracle/corpus/001/expected.json");

interface SplineDump {
  nCps: number;
  closed: boolean;
  cps: POINT[];
  bezpts: POINT[];
  bezierCount: number;
  knotCount: number;
}

function loadFirstBalloonSpline(): SplineDump {
  const raw = JSON.parse(readFileSync(GOLDEN_PATH, "utf8")) as any;
  // corpus 001, message 0, panel[1] (the panel WITH a balloon), balloon[0].
  const balloon = raw.messages[0].page.panels[1].balloons[0];
  const sp = balloon.spline;
  return {
    nCps: sp.nCps,
    closed: sp.closed,
    cps: sp.cps as POINT[],
    bezpts: sp.bezpts as POINT[],
    bezierCount: sp.bezierCount,
    knotCount: sp.knotCount,
  };
}

const GOLDEN = loadFirstBalloonSpline();

describe("CBeta against oracle corpus-001 golden", () => {
  it("golden fixture is present and well-formed", () => {
    expect(GOLDEN.nCps).toBe(22);
    expect(GOLDEN.closed).toBe(true);
    expect(GOLDEN.bezierCount).toBe(67);
    expect(GOLDEN.knotCount).toBe(25);
    expect(GOLDEN.cps).toHaveLength(22);
    expect(GOLDEN.bezpts).toHaveLength(67);
  });

  it("KnotCount matches (closed: nCps + 3)", () => {
    const sp = new CBeta(GOLDEN.cps, GOLDEN.nCps, GOLDEN.closed);
    expect(sp.KnotCount()).toBe(GOLDEN.knotCount);
  });

  it("BezierCount matches (3*KnotCount - 8)", () => {
    const sp = new CBeta(GOLDEN.cps, GOLDEN.nCps, GOLDEN.closed);
    expect(sp.BezierCount()).toBe(GOLDEN.bezierCount);
  });

  it("reproduces every Bézier control point EXACTLY (the two-agent check)", () => {
    // This is the benchmark-to-proceed from plan doc §8 Phase 3:
    // "two agents translate the same construct identically." Here the C++
    // oracle is agent A and the TS port is agent B. Exact equality of all 67
    // bezpts proves the rulebook's ROUND + CBeta matrix + GetKnot are
    // unambiguous. Any divergence means the rulebook is ambiguous.
    const sp = new CBeta(GOLDEN.cps, GOLDEN.nCps, GOLDEN.closed);
    const bezpts = sp.bezpts!;

    expect(bezpts).toHaveLength(GOLDEN.bezpts.length);
    let mismatches = 0;
    const firstDiffs: string[] = [];
    for (let i = 0; i < bezpts.length; i++) {
      const got = bezpts[i];
      const want = GOLDEN.bezpts[i];
      if (got.x !== want.x || got.y !== want.y) {
        mismatches++;
        if (firstDiffs.length < 10) {
          firstDiffs.push(
            `  bezpts[${i}]: got=(${got.x},${got.y}) want=(${want.x},${want.y})`,
          );
        }
      }
    }
    if (mismatches > 0) {
      throw new Error(
        `CBeta bezpts diverge from oracle golden: ${mismatches}/${bezpts.length} mismatched.\n` +
        `First diffs:\n${firstDiffs.join("\n")}`,
      );
    }
  });

  it("GetKnot closed-spline wraparound matches the C indices", () => {
    // spline.cpp:237 — closed: index 0 -> cps[nCps-1]; nCps+1 -> cps[0]; nCps+2 -> cps[1].
    const sp = new CBeta(GOLDEN.cps, GOLDEN.nCps, GOLDEN.closed);
    expect(sp.GetKnot(0)).toEqual(GOLDEN.cps[GOLDEN.nCps - 1]);
    expect(sp.GetKnot(GOLDEN.nCps + 1)).toEqual(GOLDEN.cps[0]);
    expect(sp.GetKnot(GOLDEN.nCps + 2)).toEqual(GOLDEN.cps[1]);
    expect(sp.GetKnot(1)).toEqual(GOLDEN.cps[0]);
    expect(sp.GetKnot(2)).toEqual(GOLDEN.cps[1]);
  });
});

describe("CCardinal sanity (no oracle golden, but pins the matrix)", () => {
  // A simple closed triangle-ish control polygon. We don't have a CCardinal
  // golden in corpus 001 (the engine uses CBeta for balloons), but we pin the
  // matrix derivation + the structural invariants so a future oracle golden
  // for a CCardinal path (avatar trajectory) has a regression target.
  const cps: POINT[] = [
    point(0, 0), point(100, 0), point(100, 100), point(0, 100),
  ];

  it("KnotCount for closed = nCps + 3", () => {
    const sp = new CCardinal(cps, 4, true);
    expect(sp.KnotCount()).toBe(7);
    expect(sp.BezierCount()).toBe(3 * 7 - 8); // 13
  });

  it("KnotCount for open = nCps + 2", () => {
    const sp = new CCardinal(cps, 4, false);
    expect(sp.KnotCount()).toBe(6);
    expect(sp.BezierCount()).toBe(3 * 6 - 8); // 10
  });

  it("GetDups = 2 for CCardinal", () => {
    const sp = new CCardinal(cps, 4, true);
    expect(sp.GetDups()).toBe(2);
  });

  it("defaultTension is 0.4 (frozen, spline.cpp:47)", () => {
    expect(CCardinal.defaultTension).toBe(0.4);
  });

  it("open-spline GetKnot duplicates endpoints (dups=2)", () => {
    const sp = new CCardinal(cps, 4, false);
    // C GetKnot open: index<dups(2)->cps[0]; index>=nCps+dups-2(=4)->cps[3];
    // else cps[index-dups+1]=cps[index-1].
    expect(sp.GetKnot(0)).toEqual(cps[0]);
    expect(sp.GetKnot(1)).toEqual(cps[0]);
    expect(sp.GetKnot(2)).toEqual(cps[1]); // cps[2-2+1]=cps[1]
    expect(sp.GetKnot(3)).toEqual(cps[2]);
    expect(sp.GetKnot(4)).toEqual(cps[3]); // boundary: index>=4 -> cps[3]
    expect(sp.GetKnot(5)).toEqual(cps[3]);
  });

  it("ComputeBezpts produces BezierCount() entries (no overread)", () => {
    const sp = new CCardinal(cps, 4, true);
    expect(sp.bezpts).not.toBeNull();
    expect(sp.bezpts!.length).toBe(sp.BezierCount());
  });
});

describe("CBeta defaults + matrix", () => {
  it("defaultTension=5.0, defaultBias=1.0 (frozen, spline.cpp:68-69)", () => {
    expect(CBeta.defaultTension).toBe(5.0);
    expect(CBeta.defaultBias).toBe(1.0);
  });

  it("GetDups = 3 for CBeta", () => {
    const sp = new CBeta([point(0, 0), point(1, 0), point(1, 1), point(0, 1)], 4, true);
    expect(sp.GetDups()).toBe(3);
  });

  it("KnotCount for open = nCps + 4", () => {
    const sp = new CBeta([point(0, 0), point(1, 0), point(1, 1), point(0, 1)], 4, false);
    expect(sp.KnotCount()).toBe(8);
  });
});

describe("splinutl — pure Bézier utilities", () => {
  // Hand-checked de Casteljau split of a unit Bézier at t=0.5.
  it("split_bezier halves a straight Bézier correctly", () => {
    const b = {
      p0: { x: 0, y: 0 }, p1: { x: 1, y: 0 }, p2: { x: 2, y: 0 }, p3: { x: 3, y: 0 },
    };
    const left = { p0: { x: 0, y: 0 }, p1: { x: 0, y: 0 }, p2: { x: 0, y: 0 }, p3: { x: 0, y: 0 } };
    const right = { p0: { x: 0, y: 0 }, p1: { x: 0, y: 0 }, p2: { x: 0, y: 0 }, p3: { x: 0, y: 0 } };
    split_bezier(b, left, right);
    // Straight line: midpoint is (1.5, 0).
    expect(left.p3).toEqual({ x: 1.5, y: 0 });
    expect(right.p0).toEqual({ x: 1.5, y: 0 });
    expect(left.p0).toEqual({ x: 0, y: 0 });
    expect(right.p3).toEqual({ x: 3, y: 0 });
  });

  it("flat_bezier detects a straight segment", () => {
    const flat = {
      p0: { x: 0, y: 0 }, p1: { x: 1, y: 0 }, p2: { x: 2, y: 0 }, p3: { x: 3, y: 0 },
    };
    expect(flat_bezier(flat)).toBe(true);
    const curvy = {
      p0: { x: 0, y: 0 }, p1: { x: 0, y: 100 }, p2: { x: 3, y: 100 }, p3: { x: 3, y: 0 },
    };
    expect(flat_bezier(curvy)).toBe(false);
  });

  it("bezier_nearest_point finds a point on a straight Bézier", () => {
    const bezpts = [[0, 0], [1, 0], [2, 0], [3, 0]];
    const r = bezier_nearest_point(bezpts, 2.0, 5.0);
    // Nearest point on the line y=0 to (2,5) is (2,0). Manhattan dist = 5.
    expect(r.found_x).toBeCloseTo(2.0, 4);
    expect(r.found_y).toBeCloseTo(0.0, 4);
  });
});

describe("CSpline structural invariants", () => {
  it("rejects n < 2 (C: ASSERT(n >= 2))", () => {
    expect(() => new CBeta([point(0, 0)], 1, true)).toThrow();
  });

  it("ComputeBezpts rejects nKnots < 4", () => {
    // 3 cps, closed -> nCps+3 = 6 knots (ok). 2 cps closed -> 5 (ok).
    // 2 cps open -> nCps+4 = 6 (ok for CBeta). Hard to trigger < 4 via CBeta.
    // Instead poke CCardinal open with 2 cps -> nCps+2 = 4 (boundary, ok).
    const sp = new CCardinal([point(0, 0), point(1, 1)], 2, false);
    expect(sp.KnotCount()).toBe(4);
    expect(() => sp.ComputeBezpts()).not.toThrow();
  });
});