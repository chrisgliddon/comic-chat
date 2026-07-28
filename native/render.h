// render.h - draw an engine-laid-out comic page with Core Graphics.
//
// This is NEW native code, not a CDC emulation, and that is a deliberate choice. The
// engine's LAYOUT is already verified byte-for-byte against Windows by the 15 Tier-3
// corpus goldens: panel rects, balloon bounding boxes, the spline control points and their
// computed Bezier points, line breaks, per-line x offsets, body rects and pose IDs are all
// exact. What the goldens do NOT cover is putting pixels on a screen.
//
// So the split is: the engine computes geometry, this file draws it. Emulating CDC through
// MFC's CPageView path instead would drag in the retained DIB section, palette realisation,
// print pagination and CScrollView - all to arrive at the same coordinates the engine has
// already handed us.
//
// SAFETY PROPERTY: nothing here is reachable from the oracle harness. The 50 goldens are
// produced by a build that never calls into this file, so no change here can move them.
// That is what makes it safe to iterate on appearance.
//
// Coordinates are MM_TWIPS as the engine produces them: 1/1440 inch, x rightward, y
// NEGATIVE downward from a page top of 0. See NativeRenderPage for the transform.

#ifndef NATIVE_RENDER_H
#define NATIVE_RENDER_H

#include <ApplicationServices/ApplicationServices.h>

// macOS's <sys/param.h> arrives via ApplicationServices and #defines MAX and MIN.
// vector2d.h:34 declares real overloaded functions with those names, so the macros turn
// `inline double MAX(double a, double b)` into a syntax error in every TU that includes
// both. Undefined here, next to the include that causes it, so the engine headers can be
// included in any order afterwards.
#undef MAX
#undef MIN

class CPage;

// Twips per pixel at 96 dpi: 1440/96. The glyph table was captured at 96 dpi, so the
// advances and this scale have to agree or text would not sit inside its balloon.
#define NATIVE_TWIPS_PER_PIXEL 15

// Draws `page` into `ctx`, which must already be sized for the page (see
// NativeRenderPageSize) and must NOT have a flip applied - this installs its own transform.
void NativeRenderPage(CPage* page, CGContextRef ctx);

// The page's pixel size, from its own bbox.
void NativeRenderPageSize(CPage* page, int* widthPx, int* heightPx);

// Convenience: render to a PNG. Used by the headless render driver, which is how the
// drawing layer is exercised without an app shell.
bool NativeRenderPageToPNG(CPage* page, const char* path);

#endif // NATIVE_RENDER_H
