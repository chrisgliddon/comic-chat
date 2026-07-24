/**
 * Value-type translations — the rulebook's §Value-Types chapter, executable.
 *
 * Source: `v1.0-pre-modern/vector2d.h` + `bbox.h` + `spline.h` + Win32 `<windef.h>`.
 * The engine uses these as plain structs (passed by value / by pointer).
 * In TS they are plain interfaces + functions operating on them; no classes,
 * no MFC inheritance — mirroring the harness README's "value types only" finding.
 *
 * Naming: C struct field names are preserved where they're lowercase in the
 * engine (RECT.left/top/right/bottom, POINT.x/y). SRECT uses the engine's
 * capitalized field names (Left/Top/Right/Bottom) so SRECT<->RECT conversion
 * is a mechanical 1:1 field copy — same as the C `SRECTToRECT`.
 */

import { INT_CAST, ROUND } from "./numeric.js";

// --- 2D points -------------------------------------------------------------
// DPOINT = double point (vector2d.h:6). POINT = Win32 integer point.
export interface DPOINT { x: number; y: number; }
export interface POINT { x: number; y: number; }

// --- Bounding boxes --------------------------------------------------------
// BOUNDBOX (vector2d.h:10) — double-precision, used by the spline utils.
export interface BOUNDBOX { xmin: number; xmax: number; ymin: number; ymax: number; }

// RECT (Win32) — integer, the engine's workhorse bbox (bbox.cpp uses left/top/right/bottom).
export interface RECT { left: number; top: number; right: number; bottom: number; }

// SRECT (bbox.h:4) — short-precision, balloon trueBox/routeRgn use this.
// Engine fields are capitalized. 16-bit range is NOT enforced in TS (the
// engine only stores values within SHORT range; if a port ever overflows it
// that's a divergence to flag, not silently wrap).
export interface SRECT { Left: number; Top: number; Right: number; Bottom: number; }

// BEZIER (vector2d.h:18) — four DPOINT control points.
export interface BEZIER { p0: DPOINT; p1: DPOINT; p2: DPOINT; p3: DPOINT; }

// --- Constructors (replace C struct-literal / POINT{x,y} init sites) -------
export function dpoint(x: number, y: number): DPOINT { return { x, y }; }
export function point(x: number, y: number): POINT { return { x, y }; }
export function rect(left: number, top: number, right: number, bottom: number): RECT {
  return { left, top, right, bottom };
}
export function srect(Left: number, Top: number, Right: number, Bottom: number): SRECT {
  return { Left, Top, Right, Bottom };
}
export function boundbox(xmin: number, xmax: number, ymin: number, ymax: number): BOUNDBOX {
  return { xmin, xmax, ymin, ymax };
}
export function bezier(p0: DPOINT, p1: DPOINT, p2: DPOINT, p3: DPOINT): BEZIER {
  return { p0, p1, p2, p3 };
}

// SRECTToRECT (bbox.cpp:103) — 1:1 field copy (short widened to int).
export function SRECTToRECT(s: SRECT): RECT {
  return { left: s.Left, top: s.Top, right: s.Right, bottom: s.Bottom };
}

// point_to_dpoint / dpoint_to_point (vector2d.cpp:155 / :162)
// dpoint_to_point uses ROUND — the load-bearing macro.
export function point_to_dpoint(pt: POINT): DPOINT { return { x: pt.x, y: pt.y }; }
export function dpoint_to_point(dpt: DPOINT): POINT { return { x: ROUND(dpt.x), y: ROUND(dpt.y) }; }