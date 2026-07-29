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

// Draws a run of CP-1252 bytes with its LEFT edge at x and its text-cell TOP at yTop,
// matching GDI's default TA_TOP|TA_LEFT alignment. Advances come from the FROZEN glyph table,
// glyph by glyph, using whichever font is currently selected there - never from Core Text, so
// drawn text lands exactly where the engine's own measurement put it. Core Text supplies the
// outlines only.
//
// Shared by native/render.cpp and the CDC text backend in native/shim/cgdraw.cpp, so there is
// one text path rather than two that can disagree.
// yDown selects the caller's vertical convention: false for the engine's page space
// (y NEGATIVE downward, no CTM flip), true for an MM_TEXT window pane (y POSITIVE downward,
// flipped CTM - see NativeWndPaint). It decides which side of the cell top the baseline
// falls on and whether the glyph outlines need countering.
void NativeDrawPinnedRun(CGContextRef ctx, const char* s, int len,
                         int x, int yTop, unsigned long color, bool yDown = false);

// --- drawing an image ---------------------------------------------------------
// Puts `img` in the rect the engine asked for, in whichever vertical convention the target
// context uses. One shared definition, because getting it wrong is close to invisible:
//
//   yDown == false  engine page space. y is NEGATIVE downward and no CTM flip is installed,
//                   so y is the rect's TOP edge and h is negative. Core Graphics normalises
//                   the rect and draws the image upright, which is what page space wants.
//
//   yDown == true   a window or offscreen surface: y POSITIVE downward with a FLIPPED CTM.
//                   The flip mirrors every image drawn through it, so it has to be undone
//                   around the draw.
//
// The trap worth recording: a NEGATIVE HEIGHT in the destination rect does NOT flip a
// CGImage. Core Graphics standardises the rect first, so (y + h, -h) and (y, h) render
// identically - unlike CGContextFillRect, where the sign is simply irrelevant. Assuming the
// negative height flipped the image is what left the self-view's head below its feet and
// mirrored the emotion wheel's face icons, which at 20x26 nearly passed for correct.
//
// Solid fills and paths need none of this: a rectangle has no up. Only images do.
static inline void NativeDrawImage(CGContextRef ctx, CGImageRef img,
                                  int x, int y, int w, int h, bool yDown) {
    if (!ctx || !img) return;
    CGContextSaveGState(ctx);
    // Nearest-neighbour: these are 1996 pixel-art bitmaps, and smoothing them reads as a
    // rendering fault rather than as anti-aliasing.
    CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
    if (yDown) {
        // Move the origin to the rect's LOWER edge and undo the flip, so the image's own
        // top lands on the upper edge - at y, where the engine put it.
        CGContextTranslateCTM(ctx, (CGFloat)x, (CGFloat)(y + h));
        CGContextScaleCTM(ctx, 1.0, -1.0);
        CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), img);
    } else {
        CGContextDrawImage(ctx, CGRectMake((CGFloat)x, (CGFloat)(y + h),
                                           (CGFloat)w, (CGFloat)(-h)), img);
    }
    CGContextRestoreGState(ctx);
}

// --- painting a real CWnd ---------------------------------------------------
// Sends `wnd` a WM_PAINT with `ctx` bound, so the window's OWN OnPaint draws it. This is how
// CBodyCam puts the self-view and the emotion wheel on screen: nothing here knows what a
// bulls-eye is, it just delivers the message and lets bodycam.cpp:OnPaint run as written.
//
// Installs the MM_TEXT transform (origin top-left, y positive downward) that a window's
// client coordinates assume, and takes it down afterwards.
class CWnd;
void NativeWndPaint(CWnd* wnd, CGContextRef ctx, int widthPx, int heightPx);

// The host installs this so a CWnd's InvalidateRect can reach -[NSView setNeedsDisplay:].
// A function pointer rather than a direct AppKit call, so the shim stays out of Objective-C
// and the headless drivers link without AppKit. The argument is the hostView a CWnd was
// attached with.
void NativeSetInvalidateHook(void (*fn)(void* hostView));

// Focus, as the engine sees it. CBodyCam::OnPaint draws a focus rect on `::GetFocus() ==
// m_hWnd`, and its key handling only makes sense for the focused pane, so the host has to say
// which window has it. Sends WM_KILLFOCUS and WM_SETFOCUS the way Windows would.
void NativeSetFocusWnd(CWnd* wnd);

// Convenience: paint a window to a PNG at a given size. Used to exercise a pane headlessly,
// which is how the self-view was verified before the app shell had a place to put it.
bool NativeWndPaintToPNG(CWnd* wnd, int widthPx, int heightPx, const char* path);

// Draws `page` into `ctx`, which must already be sized for the page (see
// NativeRenderPageSize) and must NOT have a flip applied - this installs its own transform.
void NativeRenderPage(CPage* page, CGContextRef ctx);

// The page's pixel size, from its own bbox.
void NativeRenderPageSize(CPage* page, int* widthPx, int* heightPx);

// Convenience: render to a PNG. Used by the headless render driver, which is how the
// drawing layer is exercised without an app shell.
bool NativeRenderPageToPNG(CPage* page, const char* path);

#endif // NATIVE_RENDER_H
