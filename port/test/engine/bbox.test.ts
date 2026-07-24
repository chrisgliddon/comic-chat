/**
 * Tests for engine/bbox — port of bbox.cpp.
 *
 * Covers the integer bbox ops including the pinned bug at bbox_within_bbox
 * (bbox.cpp:76 reads bbox2.bottom, not bbox1.bottom — reproduced bug-for-bug
 * per plan doc §7, marked BUG(port)).
 */
import { describe, it, expect } from "vitest";
import {
  adjust_bbox, bbox_around_pt, bbox_in_bbox, bbox_intersect, bbox_overlap,
  bbox_within_bbox, inside_bbox_r, inside_bbox_s, inside_bbox_tol, is_empty,
  make_empty_r, make_empty_s, include_pt_in_bbox_r, include_pt_in_bbox_s,
} from "../../src/engine/bbox.js";
import { SRECTToRECT, rect, srect } from "../../src/core/types.js";

describe("adjust_bbox / bbox_around_pt", () => {
  it("adjust_bbox expands by delta on all sides", () => {
    const b = rect(10, 10, 20, 0);
    adjust_bbox(b, 5);
    expect(b).toEqual(rect(5, 15, 25, -5));
  });

  it("bbox_around_pt with no delta collapses to the point", () => {
    const b = rect(0, 0, 0, 0);
    bbox_around_pt(b, { x: 7, y: 3 });
    expect(b).toEqual(rect(7, 3, 7, 3));
  });

  it("bbox_around_pt with delta expands", () => {
    const b = rect(0, 0, 0, 0);
    bbox_around_pt(b, { x: 7, y: 3 }, 2);
    expect(b).toEqual(rect(5, 5, 9, 1));
  });
});

describe("bbox_in_bbox folds source into dest", () => {
  it("expands dest to cover source", () => {
    const dest = rect(5, 5, 10, 0);
    bbox_in_bbox(rect(0, 8, 20, -5), dest);
    expect(dest).toEqual(rect(0, 8, 20, -5));
  });

  it("keeps dest when source is inside", () => {
    const dest = rect(0, 10, 20, -10);
    bbox_in_bbox(rect(5, 5, 15, -5), dest);
    expect(dest).toEqual(rect(0, 10, 20, -10));
  });
});

describe("include_pt_in_bbox", () => {
  it("RECT overload grows the box", () => {
    const b = rect(5, 5, 10, 0);
    include_pt_in_bbox_r({ x: 20, y: -10 }, b);
    expect(b).toEqual(rect(5, 5, 20, -10));
    include_pt_in_bbox_r({ x: 0, y: 20 }, b);
    expect(b).toEqual(rect(0, 20, 20, -10));
  });

  it("SRECT overload grows the box", () => {
    const b = srect(5, 5, 10, 0);
    include_pt_in_bbox_s({ x: 20, y: -10 }, b);
    expect(b).toEqual(srect(5, 5, 20, -10));
  });
});

describe("inside_bbox", () => {
  const box = rect(0, 10, 20, 0);
  it("RECT overload: inclusive bounds", () => {
    expect(inside_bbox_r({ x: 0, y: 0 }, box)).toBe(true);
    expect(inside_bbox_r({ x: 20, y: 10 }, box)).toBe(true);
    expect(inside_bbox_r({ x: -1, y: 5 }, box)).toBe(false);
    expect(inside_bbox_r({ x: 21, y: 5 }, box)).toBe(false);
  });

  it("SRECT overload", () => {
    const s = srect(0, 10, 20, 0);
    expect(inside_bbox_s({ x: 10, y: 5 }, s)).toBe(true);
    expect(inside_bbox_s({ x: 25, y: 5 }, s)).toBe(false);
  });

  it("inside_bbox_tol applies tol to the point", () => {
    // pt.x + tol >= left && pt.x - tol <= right && (same y)
    expect(inside_bbox_tol({ x: -2, y: 5 }, box, 3)).toBe(true);  // -2+3=1>=0
    expect(inside_bbox_tol({ x: -2, y: 5 }, box, 1)).toBe(false); // -2+1=-1<0
  });
});

describe("bbox_overlap", () => {
  it("overlapping boxes", () => {
    expect(bbox_overlap(rect(0, 10, 20, 0), rect(10, 20, 30, 5))).toBe(true);
  });
  it("disjoint boxes", () => {
    expect(bbox_overlap(rect(0, 10, 20, 0), rect(25, 30, 35, 20))).toBe(false);
  });
  it("touching boxes (edge-equal) overlap (inclusive)", () => {
    expect(bbox_overlap(rect(0, 10, 20, 0), rect(20, 10, 30, 0))).toBe(true);
  });
});

describe("bbox_within_bbox — BUG(port): bbox.cpp:76 reads bbox2.bottom", () => {
  it("reproduces the asymmetric containment check", () => {
    // The function tests two corners of bbox1 against bbox2:
    //   pt1 = (bbox1.right, bbox1.top)
    //   pt2 = (bbox1.left, bbox2.bottom)   <- BUG: should be bbox1.bottom
    // So a box that is "inside" by the top-right corner may still pass even
    // if its bottom-left is outside bbox2 — as long as (bbox1.left, bbox2.bottom)
    // is inside bbox2. Pin the bug.
    const outer = rect(0, 100, 100, 0);
    const inner = rect(10, 90, 90, 10);
    expect(bbox_within_bbox(inner, outer)).toBe(true);

    // Construct a case that exposes the bug: inner.bottom below outer.bottom,
    // but inner.left and bbox2.bottom (=outer.bottom=0) -> pt2=(inner.left, 0)
    // which IS inside outer, so the buggy check still returns true even though
    // inner is NOT contained (its bottom sticks out).
    const sticking = rect(10, 90, 90, -50); // bottom = -50 (below outer.bottom=0)
    // pt1 = (90, 90) inside outer? yes. pt2 = (10, outer.bottom=0) inside? yes.
    // So the buggy function returns TRUE despite sticking.bottom < outer.bottom.
    expect(bbox_within_bbox(sticking, outer)).toBe(true);
  });
});

describe("is_empty / make_empty", () => {
  it("is_empty detects inverted boxes", () => {
    expect(is_empty(rect(10, 10, 5, 5))).toBe(true);  // left > right
    expect(is_empty(rect(0, 0, 10, 10))).toBe(true);  // bottom > top
    expect(is_empty(rect(0, 10, 10, 0))).toBe(false);
  });

  it("make_empty_r sets the LARGEINTEGER sentinel (left=bottom=+LI, right=top=-LI)", () => {
    const b = rect(0, 0, 0, 0);
    make_empty_r(b);
    // C: left = bottom = LARGEINTEGER; right = top = -LARGEINTEGER
    expect(b).toEqual(rect(100000000, -100000000, -100000000, 100000000));
    expect(is_empty(b)).toBe(true); // left(1e8) > right(-1e8)
  });

  it("make_empty_s sets the LARGESHORT sentinel (Left=Bottom=+LS, Right=Top=-LS)", () => {
    const b = srect(0, 0, 0, 0);
    make_empty_s(b);
    expect(b).toEqual(srect(31000, -31000, -31000, 31000));
  });
});

describe("bbox_intersect", () => {
  it("returns is_empty(result) — true when disjoint", () => {
    const result = rect(0, 0, 0, 0);
    const empty = bbox_intersect(rect(0, 10, 10, 0), rect(20, 30, 30, 20), result);
    expect(empty).toBe(true); // disjoint -> empty intersection
  });

  it("computes the intersection rectangle when overlapping", () => {
    const result = rect(0, 0, 0, 0);
    const empty = bbox_intersect(rect(0, 10, 20, 0), rect(10, 20, 30, 5), result);
    expect(empty).toBe(false);
    expect(result).toEqual(rect(10, 10, 20, 5));
  });
});

describe("SRECTToRECT", () => {
  it("widens SRECT fields to RECT (1:1 copy)", () => {
    expect(SRECTToRECT(srect(1, 2, 3, 4))).toEqual(rect(1, 2, 3, 4));
  });
});