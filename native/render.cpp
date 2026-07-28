// render.cpp - see render.h.

#include "stdafx.h"
#include "render.h"

#include "chat.h"
#include "userinfo.h"
#include "chatprot.h"
#include "ui.h"
#include "vector2d.h"
#include "traj.h"
#include "spline.h"
#include "bbox.h"
#include "pe.h"
#include "dib.h"
#include "avbfile.h"
#include "avatar.h"
#include "balloon.h"
// panel.h names CPageView in two pure-virtual signatures without declaring it, and the
// engine .cpp files get it from their own include chains. Forward-declaring is enough -
// nothing here calls those methods, and pulling in pageview.h would drag CScrollView and
// the whole MFC view surface into the renderer for two parameter types.
class CPageView;
#include "backdrop.h"
#include "panel.h"
#include "glyphtable.h"

#include <vector>

namespace {

// --- colour -----------------------------------------------------------------------
// COLORREF is 0x00BBGGRR, which is the opposite channel order from the obvious reading.
// Getting this backwards is invisible for greys and for pure black text, and then wrong
// for every coloured nickname - so it is worth being explicit.
void SetStroke(CGContextRef c, COLORREF cr, CGFloat alpha = 1.0) {
    CGContextSetRGBStrokeColor(c, (CGFloat)GetRValue(cr) / 255.0,
                                  (CGFloat)GetGValue(cr) / 255.0,
                                  (CGFloat)GetBValue(cr) / 255.0, alpha);
}
void SetFill(CGContextRef c, COLORREF cr, CGFloat alpha = 1.0) {
    CGContextSetRGBFillColor(c, (CGFloat)GetRValue(cr) / 255.0,
                                (CGFloat)GetGValue(cr) / 255.0,
                                (CGFloat)GetBValue(cr) / 255.0, alpha);
}

// --- text -------------------------------------------------------------------------
// Comic Sans MS if the system has it, else a substitute. The FONT only affects
// appearance: every position drawn here was computed by the engine from the frozen glyph
// table, so a missing font cannot move a line break or resize a balloon. It would just
// look wrong, which is the right failure mode for a cosmetic dependency.
CTFontRef MakeFont(int lfHeight, bool italic) {
    // Size is in USER-SPACE units, and user space here is twips (the page transform scales
    // by 1/15). So the size is |lfHeight| directly - the engine's own character height.
    // Passing points instead rendered every glyph 15x too small, which read as "text is
    // missing" rather than "text is tiny".
    CGFloat pts = (CGFloat)abs(lfHeight);
    CFStringRef names[] = { CFSTR("Comic Sans MS"), CFSTR("Chalkboard"), CFSTR("Marker Felt"),
                            CFSTR("Helvetica") };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        CTFontRef f = CTFontCreateWithName(names[i], pts, NULL);
        if (!f) continue;
        CFStringRef got = CTFontCopyFamilyName(f);
        bool ok = got && CFStringCompare(got, names[i], 0) == kCFCompareEqualTo;
        if (got) CFRelease(got);
        if (ok || i + 1 == sizeof(names) / sizeof(names[0])) {
            if (italic) {
                CTFontRef it = CTFontCreateCopyWithSymbolicTraits(f, pts, NULL,
                                    kCTFontTraitItalic, kCTFontTraitItalic);
                if (it) { CFRelease(f); return it; }
            }
            return f;
        }
        CFRelease(f);
    }
    return CTFontCreateWithName(CFSTR("Helvetica"), pts, NULL);
}

// Draws one run of CP-1252 bytes with its LEFT edge at x and its text-cell TOP at yTop,
// matching GDI's default TA_TOP|TA_LEFT alignment that the engine relies on.
//
// Advances come from the frozen table, not from Core Text: the glyphs are positioned one
// at a time at the pinned offsets. Letting Core Text advance instead would drift from the
// engine's own line widths and eventually push text outside its balloon.
void DrawRun(CGContextRef c, CTFontRef font, const char* s, int len,
             int x, int yTop, int tmAscent) {
    if (!s || len <= 0) return;

    std::vector<CGGlyph>  glyphs;
    std::vector<CGPoint>  pos;
    glyphs.reserve((size_t)len);
    pos.reserve((size_t)len);

    int pen = x;
    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        // CP-1252 -> Unicode, so the 0x80-0x9F punctuation block renders as the smart
        // quotes and dashes it actually is rather than as control characters.
        UniChar u = GlyphCp1252ToUnicode(ch);
        CGGlyph g = 0;
        CTFontGetGlyphsForCharacters(font, &u, &g, 1);
        int adv = GlyphAdvance(ch);
        if (adv < 0) adv = 0;
        if (g) {
            glyphs.push_back(g);
            // Baseline sits tmAscent below the cell top. y is negative-down in engine
            // space, so the baseline is at yTop - tmAscent.
            pos.push_back(CGPointMake((CGFloat)pen, (CGFloat)(yTop - tmAscent)));
        }
        pen += adv;
    }
    if (!glyphs.empty())
        CTFontDrawGlyphs(font, &glyphs[0], &pos[0], glyphs.size(), c);
}

// --- balloon outline --------------------------------------------------------------
// The spline's bezpts are what the engine's own StrokeAndFillPath draws: bezpts[0] is the
// start point and every following group of three is one cubic segment. That is the same
// shape CGPath wants, so the balloon outline is a direct translation rather than a
// re-derivation - which matters because the corpus pins these points exactly.
CGPathRef BuildSplinePath(CSpline* sp, int dx, int dy) {
    if (!sp || !sp->bezpts) return NULL;
    int n = sp->BezierCount();
    if (n < 4) return NULL;

    CGMutablePathRef p = CGPathCreateMutable();
    CGPathMoveToPoint(p, NULL, (CGFloat)(sp->bezpts[0].x + dx), (CGFloat)(sp->bezpts[0].y + dy));
    for (int i = 1; i + 2 < n; i += 3) {
        CGPathAddCurveToPoint(p, NULL,
            (CGFloat)(sp->bezpts[i].x     + dx), (CGFloat)(sp->bezpts[i].y     + dy),
            (CGFloat)(sp->bezpts[i + 1].x + dx), (CGFloat)(sp->bezpts[i + 1].y + dy),
            (CGFloat)(sp->bezpts[i + 2].x + dx), (CGFloat)(sp->bezpts[i + 2].y + dy));
    }
    if (sp->closed) CGPathCloseSubpath(p);
    return p;
}

// Draws a CLabel's text: the balloon body, a panel title, the STARRING list. All three go
// through CLabel::Draw on Windows, so they share one routine here too.
//
// The label's OWN font is selected into the glyph table first. Titles and shouts are
// different sizes with genuinely different advances (title@4860 is lfHeight -576 against
// the balloon's -240), so measuring a title with balloon advances would spread its glyphs
// wrongly - the same mistake the table's per-font split exists to prevent.
// baseX/baseY is the origin the label's own coordinates are relative to, because the two
// cases genuinely differ:
//
//   balloon  CBWoodringNormal::Draw does OffsetWindowOrg(-m_bbox.Left, -m_bbox.Top) first,
//            so CBalloon::DrawText draws at (m_rgiLeftX[i], 0) in BALLOON-LOCAL space.
//   label    the page calls CLabel::Draw with no offset, so it draws at
//            (m_rgiLeftX[i], m_bbox.Top) in PANEL space.
//
// Collapsing the two into one origin is what put balloon text outside its balloon.
void DrawLabelText(CGContextRef c, CLabel* lab, int baseX, int baseY) {
    if (!lab || !lab->m_fontI || !lab->m_fontI->m_font) return;

    if (!GlyphSelectFont(&lab->m_fontI->m_font->m_lf)) {
        // No pinned entry for this font. Skip rather than draw with the wrong advances -
        // silently mispositioned text is harder to notice than missing text.
        return;
    }
    const GlyphMetrics* gm = GlyphTableMetrics();
    if (!gm) return;

    // The engine recomputes the line breaks at draw time (CLabel::Draw calls
    // GetFormatInfoCommon), but the balloon already holds the result of the layout pass the
    // corpus pins. Prefer that; fall back to the label's own bbox as a single line.
    CFormatInfo* fi = NULL;
    if (lab->GetType() & PE_BALLOON) fi = ((CBalloon*)lab)->m_fInfo;

    CTFontRef font = MakeFont(gm->lfHeight, gm->lfItalic != 0);
    SetFill(c, lab->m_fontI->m_crDefaultForeColor);

    if (fi && fi->m_nLines > 0) {
        int yTop = 0;                       // balloon-local; see the note above
        for (int i = 0; i < fi->m_nLines; i++) {
            DrawRun(c, font, fi->m_rgszStarts[i], fi->m_rgiLengths[i],
                    baseX + fi->m_rgiLeftX[i], baseY + yTop, gm->tmAscent);
            yTop -= lab->m_fontI->m_lineHeight;
        }
    } else if (lab->m_str) {
        DrawRun(c, font, lab->m_str, (int)strlen(lab->m_str),
                baseX + lab->m_bbox.Left, baseY + lab->m_bbox.Top, gm->tmAscent);
    }
    CFRelease(font);
}

void DrawBalloon(CGContextRef c, CBalloon* b, int panelLeft, int panelTop) {
    // Element bboxes are panel-relative and the spline is relative to the balloon, so both
    // offsets apply.
    int bx = panelLeft + b->m_bbox.Left;
    int by = panelTop  + b->m_bbox.Top;

    CGPathRef path = BuildSplinePath(b->m_spline, bx, by);
    if (path) {
        CGContextAddPath(c, path);
        SetFill(c, RGB(255, 255, 255));
        CGContextFillPath(c);

        CGContextAddPath(c, path);
        SetStroke(c, RGB(0, 0, 0));
        CGContextSetLineWidth(c, 22.0);      // ~1.5px at 15 twips/px
        CGContextStrokePath(c);
        CFRelease(path);
    }
    DrawLabelText(c, b, bx, by);   // balloon-local origin
}

// --- avatar body ------------------------------------------------------------------
// Drawn by the ENGINE, not here. CBody::Draw dispatches to CBodyDouble/CBodySingle::DrawBody
// (bodycam.cpp:516-607), which picks the head and torso poses from the pose IDs the corpus
// pins, computes their rects, honours the flip, and blits mask-then-drawing in the order the
// avatar's TORSOFIRST/TORSOMASK/HEADMASK flags dictate. All of that reaches the screen
// through ::StretchDIBits, which native/shim/cgblit.cpp now implements against a CGContext.
//
// So the body compositing is the engine's own code path, and this only has to hand it a DC
// pointing at the right context. Reimplementing it here would have meant re-deriving flag
// handling and mask semantics that already exist and are already correct.
void DrawBody(CGContextRef c, CBody* body, int panelLeft, int panelTop) {
    if (!body) return;

    // A DC whose only job is to carry the context. Painting members are no-ops in the shim
    // except the blitter, which is all a body needs.
    CDC dc;
    dc.m_cgCtx = (void*)c;

    CGContextSaveGState(c);
    // CBody::Draw works in panel-relative coordinates (its m_bbox is panel-relative and
    // DrawBody passes SRECTToRECT(m_bbox) straight through), so the panel origin is applied
    // as a transform rather than added at every call site.
    CGContextTranslateCTM(c, panelLeft, panelTop);
    POINT ul; ul.x = 0; ul.y = 0;
    body->Draw(&dc, &ul, NULL);
    CGContextRestoreGState(c);
}

// --- panel ------------------------------------------------------------------------
// A CPanel carries NO position: the page places it, passing the origin as `ul` to
// CPanel::Draw. CUnitPanelPage::Draw (panel.cpp:1307-1329) walks the list stepping
// loc.x by m_unitWidth + m_vInterstice and wrapping every m_panelsPerRow rows by
// m_unitHeight + m_hInterstice. Mirrored exactly here, from the page's own statics, so
// panel placement cannot drift from the engine's.
void DrawPanel(CGContextRef c, CPanel* panel, int px, int py) {
    int w = CUnitPanelPage::m_unitWidth;
    int h = CUnitPanelPage::m_unitHeight;

    // Panel background. Backdrop art is a separate step (see the body note above); white
    // keeps the page readable until it is wired up.
    SetFill(c, RGB(255, 255, 255));
    CGContextFillRect(c, CGRectMake(px, py - h, w, h));

    if (panel->m_hasBorder) {
        SetStroke(c, RGB(0, 0, 0));
        CGContextSetLineWidth(c, (CGFloat)CUnitPanel::m_borderWidth);
        CGContextStrokeRect(c, CGRectMake(px, py - h, w, h));
    }

    // Clip to the panel. The engine gets this for free by drawing each panel into a
    // panel-sized memory DC and blitting the result (panel.cpp:1303-1321); drawing straight
    // onto the page instead means the clip has to be explicit, or a tall balloon spills
    // into its neighbour.
    CGContextSaveGState(c);
    CGContextClipToRect(c, CGRectMake(px, py - h, w, h));

    // Bodies first, then balloons and labels over them - the engine's own z-order.
    POSITION bp = panel->m_bodies.GetHeadPosition();
    while (bp) DrawBody(c, (CBody*)panel->m_bodies.GetNext(bp), px, py);

    POSITION ep = panel->m_elements.GetHeadPosition();
    while (ep) {
        CPanelElement* e = (CPanelElement*)panel->m_elements.GetNext(ep);
        int t = e->GetType();
        if (t & PE_BALLOON) {
            DrawBalloon(c, (CBalloon*)e, px, py);
        } else if (t == PE_UNKNOWN) {
            // A plain CLabel: the panel title and the STARRING list. CBody and CBackDrop
            // also derive from CPanelElement and also report PE_UNKNOWN, but they live in
            // m_bodies / m_backDrop rather than m_elements, so this list is labels only.
            DrawLabelText(c, (CLabel*)e, px, py);
        }
    }
    CGContextRestoreGState(c);
}

} // namespace

void NativeRenderPageSize(CPage* page, int* widthPx, int* heightPx) {
    RECT bb;
    page->GetBBox(&bb);
    int w = (int)(bb.right - bb.left);
    int h = (int)(bb.top - bb.bottom);       // top is 0, bottom negative
    if (widthPx)  *widthPx  = (w + NATIVE_TWIPS_PER_PIXEL - 1) / NATIVE_TWIPS_PER_PIXEL;
    if (heightPx) *heightPx = (h + NATIVE_TWIPS_PER_PIXEL - 1) / NATIVE_TWIPS_PER_PIXEL;
}

void NativeRenderPage(CPage* page, CGContextRef ctx) {
    if (!page || !ctx) return;

    int wpx, hpx;
    NativeRenderPageSize(page, &wpx, &hpx);

    // Engine space -> device space, in one transform so every draw below can use the
    // engine's own twips unchanged:
    //   x_dev = x_twips / 15
    //   y_dev = hpx + y_twips / 15        (y_twips is 0 at the page top, negative downward)
    // Verified by the ends: y_twips 0 -> hpx (top), y_twips -pageHeight -> 0 (bottom).
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, (CGFloat)hpx);
    CGContextScaleCTM(ctx, 1.0 / NATIVE_TWIPS_PER_PIXEL, 1.0 / NATIVE_TWIPS_PER_PIXEL);

    // Page background: the gutter between panels.
    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    RECT pb;
    page->GetBBox(&pb);
    CGContextFillRect(ctx, CGRectMake(pb.left, pb.bottom,
                                      pb.right - pb.left, pb.top - pb.bottom));

    if (getenv("COMIC_CHAT_RENDER_TRACE"))
        fprintf(stderr, "render: page bbox=(%d,%d,%d,%d) %dx%d px; unit=%dx%d perRow=%d "
                        "interstice v=%d h=%d panels=%d\n",
                (int)pb.left, (int)pb.top, (int)pb.right, (int)pb.bottom, wpx, hpx,
                CUnitPanelPage::m_unitWidth, CUnitPanelPage::m_unitHeight,
                CUnitPanelPage::m_panelsPerRow, CUnitPanelPage::m_vInterstice,
                CUnitPanelPage::m_hInterstice, (int)page->m_panels.GetCount());

    CGContextSetLineJoin(ctx, kCGLineJoinRound);
    CGContextSetLineCap(ctx, kCGLineCapRound);

    int count = 0;
    int locx = (int)pb.left, locy = (int)pb.top;
    const int perRow = CUnitPanelPage::m_panelsPerRow;
    POSITION pos = page->m_panels.GetHeadPosition();
    while (pos) {
        CPanel* panel = (CPanel*)page->m_panels.GetNext(pos);
        DrawPanel(ctx, panel, locx, locy);
        if (++count % perRow == 0) {
            locx = (int)pb.left;
            locy -= CUnitPanelPage::m_unitHeight + CUnitPanelPage::m_hInterstice;
        } else {
            locx += CUnitPanelPage::m_unitWidth + CUnitPanelPage::m_vInterstice;
        }
    }

    CGContextRestoreGState(ctx);
}

bool NativeRenderPageToPNG(CPage* page, const char* path) {
    if (!page || !path) return false;

    int wpx, hpx;
    NativeRenderPageSize(page, &wpx, &hpx);
    if (wpx <= 0 || hpx <= 0) return false;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(NULL, wpx, hpx, 8, 0, cs,
                                             kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    if (!ctx) return false;

    NativeRenderPage(page, ctx);

    CGImageRef img = CGBitmapContextCreateImage(ctx);
    bool ok = false;
    if (img) {
        CFStringRef sp = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
        CFURLRef url = CFURLCreateWithFileSystemPath(NULL, sp, kCFURLPOSIXPathStyle, false);
        CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, NULL);
        if (dst) {
            CGImageDestinationAddImage(dst, img, NULL);
            ok = CGImageDestinationFinalize(dst);
            CFRelease(dst);
        }
        CFRelease(url);
        CFRelease(sp);
        CFRelease(img);
    }
    CFRelease(ctx);
    return ok;
}
