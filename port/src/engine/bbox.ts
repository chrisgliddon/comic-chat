/**
 * Port of `bbox.cpp` — integer bounding-box operations.
 *
 * Source: `v1.0-pre-modern/bbox.cpp` (byte-identical to the v2.5 oracle tree
 * modulo the copyright header). All functions mutate their `bbox` argument
 * in place, mirroring the C pointer-mutation signature (`RECT *bbox`).
 * Callers pass the box by reference (objects are references in JS).
 *
 * Win32 `min`/`max` macros map to Math.min/Math.max. `BOOL` -> boolean.
 * `LARGEINTEGER` / `LARGESHORT` come from core/numeric.
 */

import { LARGEINTEGER, LARGESHORT } from "../core/numeric.js";
import type { POINT, RECT, SRECT } from "../core/types.js";

// adjust_bbox (bbox.cpp:8)
export function adjust_bbox(bbox: RECT, delta: number): void {
  bbox.left -= delta;
  bbox.bottom -= delta;
  bbox.right += delta;
  bbox.top += delta;
}

// bbox_around_pt (bbox.cpp:15) — C default arg `delta=0` -> optional param.
export function bbox_around_pt(bbox: RECT, pt: POINT, delta = 0): void {
  bbox.left = bbox.right = pt.x;
  bbox.top = bbox.bottom = pt.y;
  if (delta) adjust_bbox(bbox, delta);
}

// bbox_in_bbox (bbox.cpp:21) — folds source into dest.
export function bbox_in_bbox(source: RECT, dest: RECT): void {
  dest.left = Math.min(source.left, dest.left);
  dest.bottom = Math.min(source.bottom, dest.bottom);
  dest.right = Math.max(source.right, dest.right);
  dest.top = Math.max(source.top, dest.top);
}

// include_pt_in_bbox — RECT overload (bbox.cpp:28)
export function include_pt_in_bbox_r(pt: POINT, bbox: RECT): void {
  bbox.left = Math.min(pt.x, bbox.left);
  bbox.bottom = Math.min(pt.y, bbox.bottom);
  bbox.right = Math.max(pt.x, bbox.right);
  bbox.top = Math.max(pt.y, bbox.top);
}

// include_pt_in_bbox — SRECT overload (bbox.cpp:35). C casts to `(short)` after
// min/max; we keep the int result. The engine only stores short-range values
// here; a port-time overflow would flag a divergence (RULEBOOK §SRECT-range).
export function include_pt_in_bbox_s(pt: POINT, bbox: SRECT): void {
  bbox.Left = Math.min(pt.x, bbox.Left);
  bbox.Bottom = Math.min(pt.y, bbox.Bottom);
  bbox.Right = Math.max(pt.x, bbox.Right);
  bbox.Top = Math.max(pt.y, bbox.Top);
}

// inside_bbox — RECT overload (bbox.cpp:42)
export function inside_bbox_r(pt: POINT, bbox: RECT): boolean {
  return (
    pt.x >= bbox.left &&
    pt.x <= bbox.right &&
    pt.y >= bbox.bottom &&
    pt.y <= bbox.top
  );
}

// inside_bbox — SRECT overload (bbox.cpp:49)
export function inside_bbox_s(pt: POINT, bbox: SRECT): boolean {
  return (
    pt.x >= bbox.Left &&
    pt.x <= bbox.Right &&
    pt.y >= bbox.Bottom &&
    pt.y <= bbox.Top
  );
}

// inside_bbox_tol (bbox.cpp:56) — tolerance applied to the point, not the box.
export function inside_bbox_tol(pt: POINT, bbox: RECT, tol: number): boolean {
  return (
    pt.x + tol >= bbox.left &&
    pt.x - tol <= bbox.right &&
    pt.y + tol >= bbox.bottom &&
    pt.y - tol <= bbox.top
  );
}

// bbox_overlap (bbox.cpp:63)
export function bbox_overlap(bbox1: RECT, bbox2: RECT): boolean {
  return !(
    bbox1.left > bbox2.right ||
    bbox2.left > bbox1.right ||
    bbox1.bottom > bbox2.top ||
    bbox2.bottom > bbox1.top
  );
}

// bbox_within_bbox (bbox.cpp:71) — NOTE the original has a likely bug:
// `pt2.y = bbox2->bottom` (reads bbox2, not bbox1). We reproduce bug-for-bug
// per plan doc §7 (BUG(port): bbox.cpp:76 bbox2/bbox1 typo). The two corners
// tested are bbox1's top-right and (bbox1.left, bbox2.bottom) — an asymmetric
// containment check. Pinned, not fixed.
export function bbox_within_bbox(bbox1: RECT, bbox2: RECT): boolean {
  const pt1: POINT = { x: bbox1.right, y: bbox1.top };
  const pt2: POINT = { x: bbox1.left, y: bbox2.bottom };
  return inside_bbox_tol(pt1, bbox2, 0) && inside_bbox_tol(pt2, bbox2, 0);
}

// is_empty (bbox.cpp:81)
export function is_empty(bbox: RECT): boolean {
  return bbox.left > bbox.right || bbox.bottom > bbox.top;
}

// make_empty — RECT overload (bbox.cpp:85)
export function make_empty_r(bbox: RECT): void {
  bbox.left = bbox.bottom = LARGEINTEGER;
  bbox.right = bbox.top = -LARGEINTEGER;
}

// make_empty — SRECT overload (bbox.cpp:90)
export function make_empty_s(bbox: SRECT): void {
  bbox.Left = bbox.Bottom = LARGESHORT;
  bbox.Right = bbox.Top = -LARGESHORT;
}

// bbox_intersect (bbox.cpp:95) — returns whether the result is EMPTY (the
// C function returns `is_empty(result)`, i.e. TRUE when boxes don't overlap).
// We return that same boolean AND write the intersection into `result`.
export function bbox_intersect(bbox1: RECT, bbox2: RECT, result: RECT): boolean {
  result.left = Math.max(bbox1.left, bbox2.left);
  result.right = Math.min(bbox1.right, bbox2.right);
  result.top = Math.min(bbox1.top, bbox2.top);
  result.bottom = Math.max(bbox1.bottom, bbox2.bottom);
  return is_empty(result);
}

// SRECTToRECT lives in core/types; re-export for header parity.
export { SRECTToRECT } from "../core/types.js";