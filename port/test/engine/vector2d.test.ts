/**
 * Tests for engine/vector2d — port of vector2d.cpp.
 *
 * Pure-math property tests + golden checks against hand-computed values that
 * exercise the ROUND / INT_CAST boundaries. The full float-vs-oracle golden
 * lives in spline.test.ts (corpus-001), which exercises vector2d indirectly
 * through CvertsToCubic.
 */
import { describe, it, expect } from "vitest";
import {
  add_angles, angle_between_vecs, angle_to_vector, degrees_to_rads,
  point_add_d, point_add_i, point_dist_d, point_dist_i, point_distsq_d,
  point_distsq_i, point_dot_d, point_dot_i, point_magn_d, point_magn_i,
  point_norm_d, point_norm_i, point_scalmult_d, point_scalmult_i,
  point_sub_d, point_sub_i, subtract_angles, value_to_angle,
  vector_to_angle_d,
} from "../../src/engine/vector2d.js";
import { dpoint_to_point, point_to_dpoint } from "../../src/core/types.js";
import { ROUND } from "../../src/core/numeric.js";

describe("DPOINT arithmetic", () => {
  it("point_add_d / point_sub_d are inverses", () => {
    const a = { x: 3.5, y: -2.25 };
    const b = { x: 1.25, y: 7.75 };
    const sum = point_add_d(a, b);
    expect(sum).toEqual({ x: 4.75, y: 5.5 });
    expect(point_sub_d(sum, b)).toEqual(a);
  });

  it("point_scalmult_d scales both components", () => {
    expect(point_scalmult_d(2, { x: 3, y: -4 })).toEqual({ x: 6, y: -8 });
    expect(point_scalmult_d(0.5, { x: 1, y: 1 })).toEqual({ x: 0.5, y: 0.5 });
  });

  it("point_dot_d", () => {
    expect(point_dot_d({ x: 1, y: 0 }, { x: 0, y: 1 })).toBe(0);
    expect(point_dot_d({ x: 3, y: 4 }, { x: 3, y: 4 })).toBe(25);
  });

  it("point_dist_d / point_distsq_d", () => {
    expect(point_dist_d({ x: 0, y: 0 }, { x: 3, y: 4 })).toBe(5);
    expect(point_distsq_d({ x: 0, y: 0 }, { x: 3, y: 4 })).toBe(25);
  });

  it("point_magn_d", () => {
    expect(point_magn_d({ x: 3, y: 4 })).toBe(5);
    expect(point_magn_d({ x: 0, y: 0 })).toBe(0);
  });

  it("point_norm_d unitizes (and zeroes the degenerate)", () => {
    const n = point_norm_d({ x: 3, y: 4 });
    expect(n.x).toBeCloseTo(0.6, 10);
    expect(n.y).toBeCloseTo(0.8, 10);
    expect(point_norm_d({ x: 0, y: 0 })).toEqual({ x: 0, y: 0 });
  });
});

describe("POINT (integer) arithmetic", () => {
  it("point_scalmult_i uses INT_CAST (truncation toward zero), NOT round", () => {
    // vector2d.cpp:100 — `(int)(pt.x * scalar)`. Truncation toward zero.
    expect(point_scalmult_i(0.5, { x: 3, y: 4 })).toEqual({ x: 1, y: 2 });
    expect(point_scalmult_i(0.5, { x: -3, y: -4 })).toEqual({ x: -1, y: -2 });
    // 2.9 * 1 = 2.9 -> trunc = 2 (not round-to-3)
    expect(point_scalmult_i(1, { x: 3, y: 0 })).toEqual({ x: 3, y: 0 });
    // -2.9 -> trunc = -2 (toward zero, not floor -3)
    expect(point_scalmult_i(-1, { x: 3, y: 0 })).toEqual({ x: -3, y: 0 });
  });

  it("point_add_i / point_sub_i", () => {
    expect(point_add_i({ x: 1, y: 2 }, { x: 3, y: 4 })).toEqual({ x: 4, y: 6 });
    expect(point_sub_i({ x: 5, y: 5 }, { x: 2, y: 1 })).toEqual({ x: 3, y: 4 });
  });

  it("point_dist_i / point_distsq_i / point_dot_i / point_magn_i", () => {
    expect(point_dist_i({ x: 0, y: 0 }, { x: 6, y: 8 })).toBe(10);
    expect(point_distsq_i({ x: 0, y: 0 }, { x: 6, y: 8 })).toBe(100);
    expect(point_dot_i({ x: 2, y: 3 }, { x: 4, y: 5 })).toBe(23);
    expect(point_magn_i({ x: 6, y: 8 })).toBe(10);
  });

  it("dpoint_to_point uses ROUND (load-bearing, oracle-pinned floor(fp+0.5))", () => {
    // ROUND(2.5) = floor(3.0) = 3; ROUND(-2.6) = floor(-2.1) = -3
    expect(dpoint_to_point({ x: 2.5, y: -2.6 })).toEqual({ x: 3, y: -3 });
    // ROUND(1.4) = floor(1.9) = 1; ROUND(-1.4) = floor(-0.9) = -1
    expect(dpoint_to_point({ x: 1.4, y: -1.4 })).toEqual({ x: 1, y: -1 });
  });

  it("point_to_dpoint is a widening no-op", () => {
    expect(point_to_dpoint({ x: 5, y: -3 })).toEqual({ x: 5, y: -3 });
  });
});

describe("angle routines", () => {
  it("degrees_to_rads uses the low-precision PI=3.14159, NOT Math.PI", () => {
    // 180 deg -> PI = 3.14159 (not 3.141592653589793). Pin the literal.
    expect(degrees_to_rads(180)).toBe(3.14159);
    expect(degrees_to_rads(180)).not.toBe(Math.PI);
  });

  it("angle_to_vector / vector_to_angle_d round-trip", () => {
    for (const ang of [0, 0.5, 1.0, -1.0, 2.5, -2.5, Math.PI / 2]) {
      const v = angle_to_vector(ang);
      const back = vector_to_angle_d(v);
      expect(back).toBeCloseTo(ang, 6);
    }
  });

  it("value_to_angle normalizes to (-PI, PI]", () => {
    // value_to_angle(PI) = PI (boundary inclusive)
    expect(value_to_angle(3.14159)).toBe(3.14159);
    // value_to_angle(-PI) should wrap to +PI (the interval is (-PI, PI])
    expect(value_to_angle(-3.14159)).toBeCloseTo(3.14159, 6);
    // 2*PI -> 0
    expect(value_to_angle(2 * 3.14159)).toBeCloseTo(0, 6);
    // -2*PI -> 0
    expect(value_to_angle(-2 * 3.14159)).toBeCloseTo(0, 6);
  });

  it("add_angles / subtract_angles wrap", () => {
    // add 3*PI + PI = 4*PI -> 0
    expect(add_angles(3 * 3.14159, 3.14159)).toBeCloseTo(0, 5);
    // subtract PI from -PI -> -2*PI -> 0
    expect(subtract_angles(-3.14159, 3.14159)).toBeCloseTo(0, 5);
  });

  it("angle_between_vecs", () => {
    // angle from x-axis to y-axis is +PI/2
    const a = angle_between_vecs({ x: 1, y: 0 }, { x: 0, y: 1 });
    expect(a).toBeCloseTo(Math.PI / 2, 4);
  });

  it("vector_to_angle_d zeroes the degenerate", () => {
    expect(vector_to_angle_d({ x: 0, y: 0 })).toBe(0);
  });

  it("ROUND re-export parity check (sanity)", () => {
    // Confirm ROUND is the oracle-pinned floor(fp+0.5) form.
    expect(ROUND(-2.6)).toBe(-3); // floor(-2.1) = -3 (matches Math.round)
    expect(ROUND(-61.647)).toBe(-62); // the corpus-001 bezpts[0].x raw dot
  });
});