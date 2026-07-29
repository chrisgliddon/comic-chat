// cgdraw.cpp - CDC's path, text and fill members over Core Graphics.
//
// The point of this file is that the ENGINE draws its own balloons. CBWoodringNormal::Draw
// (balloon.cpp:1789) is:
//
//     SelectObject(m_byteDashed ? &m_nimbusPen : &m_pen);
//     OffsetWindowOrg(-m_bbox.Left, -m_bbox.Top);
//     m_traj->Draw(pdc);              // BeginPath / MoveTo / PolyBezier / CloseFigure / EndPath
//     brush.CreateSolidBrush(RGB(255,255,255));  SelectObject(&brush);
//     StrokeAndFillPath();
//     if (m_byteDashed) { SelectObject(&m_pen); m_traj->Dash(pdc); }
//     SetBkMode(TRANSPARENT);  DrawText(pdc);
//
// Backing those calls rather than reimplementing them is what produces the balloon TAIL. The
// tail is not part of m_spline - it is a segment of m_traj (traj.cpp:45 walks m_segs, which is
// the body and the tail together), so a renderer that drew the spline got a balloon that
// pointed at nobody. It also gets thought-balloon ovals, whisper dashes and the halo pass for
// free, because those are all just different calls into the same API.
//
// SAFETY: every entry point returns immediately when m_cgCtx is NULL, which is the case for
// every DC the oracle harness creates. No golden can be moved from here.

#include "stdafx.h"
#include "render.h"          // ApplicationServices, and the MAX/MIN #undef

#include <vector>

namespace {

// Logical -> drawing coordinates. GDI computes device = logical - windowOrg, and the engine
// uses OffsetWindowOrg to express balloon contents in balloon-local space.
inline CGFloat LX(const CDC* dc, int x) { return (CGFloat)(x - dc->m_winOrgX); }
inline CGFloat LY(const CDC* dc, int y) { return (CGFloat)(y - dc->m_winOrgY); }

// onePixel: how many LOGICAL units make one device pixel in the calling DC. It is 15 in
// MM_TWIPS page space (1440/96) and 1 in an MM_TEXT window, where logical units already ARE
// pixels. This has to be passed in rather than assumed: a GDI pen of width 0 means "one
// device pixel", and hardcoding the twips answer drew the emotion wheel's outline 15 pixels
// thick - a black annulus instead of a thin circle. The dash lengths scale the same way.
void ApplyStroke(CGContextRef c, const CPen* pen, int onePixel) {
    COLORREF col = pen ? pen->m_color : RGB(0, 0, 0);
    CGContextSetRGBStrokeColor(c, (CGFloat)GetRValue(col) / 255.0,
                                  (CGFloat)GetGValue(col) / 255.0,
                                  (CGFloat)GetBValue(col) / 255.0, 1.0);
    int w = pen ? pen->m_width : 0;
    CGContextSetLineWidth(c, (CGFloat)(w > 0 ? w : onePixel));
    // PS_DASH is 1, PS_DOT 2, PS_DASHDOT 3. Whisper balloons use a dashed "nimbus" pen, which
    // is exactly how the paper describes them, so the style has to survive.
    if (pen && pen->m_style >= 1 && pen->m_style <= 3) {
        CGFloat dash[2] = { (CGFloat)(4 * onePixel), (CGFloat)(4 * onePixel) };
        CGContextSetLineDash(c, 0, dash, 2);
    } else {
        CGContextSetLineDash(c, 0, NULL, 0);
    }
}

void ApplyFill(CGContextRef c, const CBrush* br) {
    COLORREF col = br ? br->m_color : RGB(255, 255, 255);
    CGContextSetRGBFillColor(c, (CGFloat)GetRValue(col) / 255.0,
                                (CGFloat)GetGValue(col) / 255.0,
                                (CGFloat)GetBValue(col) / 255.0, 1.0);
}

} // namespace

// --- path construction --------------------------------------------------------------

BOOL CDC::BeginPath() {
    if (!m_cgCtx) return TRUE;
    if (m_cgPath) CFRelease((CGMutablePathRef)m_cgPath);
    m_cgPath = CGPathCreateMutable();
    m_hasCur = 0;
    return TRUE;
}

BOOL CDC::EndPath() { return TRUE; }   // the path stays until it is painted

BOOL CDC::MoveTo(int x, int y) {
    m_curX = x; m_curY = y;
    if (!m_cgCtx) return TRUE;
    if (m_cgPath) {
        CGPathMoveToPoint((CGMutablePathRef)m_cgPath, NULL, LX(this, x), LY(this, y));
        m_hasCur = 1;
    }
    return TRUE;
}

BOOL CDC::LineTo(int x, int y) {
    if (!m_cgCtx) { m_curX = x; m_curY = y; return TRUE; }
    if (m_cgPath) {
        // A LineTo with no current point starts the figure, matching GDI's behaviour of
        // treating the current position as the origin.
        if (!m_hasCur) {
            CGPathMoveToPoint((CGMutablePathRef)m_cgPath, NULL, LX(this, m_curX), LY(this, m_curY));
            m_hasCur = 1;
        }
        CGPathAddLineToPoint((CGMutablePathRef)m_cgPath, NULL, LX(this, x), LY(this, y));
    } else {
        // OUTSIDE a path, GDI's LineTo draws immediately with the current pen. This is not a
        // corner case: DashSeg (traj.cpp:10) draws whisper-balloon dashes as bare
        // MoveTo/LineTo pairs with no BeginPath, so a LineTo that only ever appended to a path
        // would silently drop every dash.
        CGContextRef c = (CGContextRef)m_cgCtx;
        CGContextSaveGState(c);
        CGContextSetLineCap(c, kCGLineCapRound);
        CGContextBeginPath(c);
        CGContextMoveToPoint(c, LX(this, m_curX), LY(this, m_curY));
        CGContextAddLineToPoint(c, LX(this, x), LY(this, y));
        ApplyStroke(c, m_pPen, OnePixel());
        CGContextStrokePath(c);
        CGContextRestoreGState(c);
    }
    m_curX = x; m_curY = y;
    return TRUE;
}

// GDI's PolyBezier takes a START POINT followed by groups of three; PolyBezierTo omits the
// start and continues from the current point. traj.cpp uses both.
BOOL CDC::PolyBezier(const POINT* pts, int n) {
    if (!pts || n < 4) return FALSE;
    if (!m_cgCtx) { m_curX = (int)pts[n-1].x; m_curY = (int)pts[n-1].y; return TRUE; }
    if (m_cgPath) {
        CGPathMoveToPoint((CGMutablePathRef)m_cgPath, NULL, LX(this, (int)pts[0].x), LY(this, (int)pts[0].y));
        m_hasCur = 1;
        for (int i = 1; i + 2 < n; i += 3)
            CGPathAddCurveToPoint((CGMutablePathRef)m_cgPath, NULL,
                LX(this, (int)pts[i].x),     LY(this, (int)pts[i].y),
                LX(this, (int)pts[i+1].x),   LY(this, (int)pts[i+1].y),
                LX(this, (int)pts[i+2].x),   LY(this, (int)pts[i+2].y));
    }
    m_curX = (int)pts[n-1].x; m_curY = (int)pts[n-1].y;
    return TRUE;
}

BOOL CDC::PolyBezierTo(const POINT* pts, int n) {
    if (!pts || n < 3) return FALSE;
    if (!m_cgCtx) { m_curX = (int)pts[n-1].x; m_curY = (int)pts[n-1].y; return TRUE; }
    if (m_cgPath) {
        if (!m_hasCur) {
            CGPathMoveToPoint((CGMutablePathRef)m_cgPath, NULL, LX(this, m_curX), LY(this, m_curY));
            m_hasCur = 1;
        }
        for (int i = 0; i + 2 < n; i += 3)
            CGPathAddCurveToPoint((CGMutablePathRef)m_cgPath, NULL,
                LX(this, (int)pts[i].x),     LY(this, (int)pts[i].y),
                LX(this, (int)pts[i+1].x),   LY(this, (int)pts[i+1].y),
                LX(this, (int)pts[i+2].x),   LY(this, (int)pts[i+2].y));
    }
    m_curX = (int)pts[n-1].x; m_curY = (int)pts[n-1].y;
    return TRUE;
}

BOOL CDC::CloseFigure() {
    if (m_cgCtx && m_cgPath) CGPathCloseSubpath((CGMutablePathRef)m_cgPath);
    return TRUE;
}

BOOL CDC::Ellipse(int l, int t, int r, int b) {
    if (!m_cgCtx) return TRUE;
    CGRect rc = CGRectMake(LX(this, l), LY(this, b),
                           (CGFloat)(r - l), (CGFloat)(t - b));
    if (m_cgPath) {
        CGPathAddEllipseInRect((CGMutablePathRef)m_cgPath, NULL, rc);
    } else {
        // Outside a path, Ellipse both fills and strokes immediately. Thought balloons draw
        // their tail as a row of such ovals.
        CGContextRef c = (CGContextRef)m_cgCtx;
        CGContextSaveGState(c);
        CGContextAddEllipseInRect(c, rc);
        ApplyFill(c, m_pBrush);
        if (!m_pBrush || !m_pBrush->m_null) CGContextFillPath(c);
        CGContextAddEllipseInRect(c, rc);
        ApplyStroke(c, m_pPen, OnePixel());
        CGContextStrokePath(c);
        CGContextRestoreGState(c);
    }
    return TRUE;
}

BOOL CDC::Ellipse(const RECT* rc) {
    return rc ? Ellipse((int)rc->left, (int)rc->top, (int)rc->right, (int)rc->bottom) : FALSE;
}

// --- painting -----------------------------------------------------------------------

BOOL CDC::StrokeAndFillPath() {
    if (!m_cgCtx || !m_cgPath) return TRUE;
    CGContextRef c = (CGContextRef)m_cgCtx;
    CGPathRef path = (CGPathRef)m_cgPath;

    CGContextSaveGState(c);
    CGContextSetLineJoin(c, kCGLineJoinRound);
    CGContextSetLineCap(c, kCGLineCapRound);

    // Fill first, then stroke, so the outline is not half-covered by the fill - which is what
    // GDI's StrokeAndFillPath produces and what a comic balloon needs (opaque interior hiding
    // the background, crisp black outline on top).
    if (!m_pBrush || !m_pBrush->m_null) {
        CGContextAddPath(c, path);
        ApplyFill(c, m_pBrush);
        CGContextFillPath(c);
    }
    CGContextAddPath(c, path);
    ApplyStroke(c, m_pPen, OnePixel());
    CGContextStrokePath(c);
    CGContextRestoreGState(c);

    CFRelease(path);
    m_cgPath = 0;
    m_hasCur = 0;
    return TRUE;
}

BOOL CDC::StrokePath() {
    if (!m_cgCtx || !m_cgPath) return TRUE;
    CGContextRef c = (CGContextRef)m_cgCtx;
    CGContextSaveGState(c);
    CGContextSetLineJoin(c, kCGLineJoinRound);
    CGContextSetLineCap(c, kCGLineCapRound);
    CGContextAddPath(c, (CGPathRef)m_cgPath);
    ApplyStroke(c, m_pPen, OnePixel());
    CGContextStrokePath(c);
    CGContextRestoreGState(c);
    CFRelease((CGPathRef)m_cgPath);
    m_cgPath = 0;
    m_hasCur = 0;
    return TRUE;
}

BOOL CDC::FillPath() {
    if (!m_cgCtx || !m_cgPath) return TRUE;
    CGContextRef c = (CGContextRef)m_cgCtx;
    CGContextSaveGState(c);
    CGContextAddPath(c, (CGPathRef)m_cgPath);
    ApplyFill(c, m_pBrush);
    CGContextFillPath(c);
    CGContextRestoreGState(c);
    CFRelease((CGPathRef)m_cgPath);
    m_cgPath = 0;
    m_hasCur = 0;
    return TRUE;
}

void CDC::FillSolidRect(int x, int y, int w, int h, COLORREF col) {
    if (!m_cgCtx) return;
    CGContextRef c = (CGContextRef)m_cgCtx;
    CGContextSaveGState(c);
    CGContextSetRGBFillColor(c, (CGFloat)GetRValue(col) / 255.0,
                                (CGFloat)GetGValue(col) / 255.0,
                                (CGFloat)GetBValue(col) / 255.0, 1.0);
    // Engine rects run y-negative-downward, so the CG rect origin is the bottom edge.
    CGContextFillRect(c, CGRectMake(LX(this, x), LY(this, y + h), (CGFloat)w, (CGFloat)-h));
    CGContextRestoreGState(c);
}

void CDC::FillSolidRect(const RECT* rc, COLORREF col) {
    if (!rc) return;
    FillSolidRect((int)rc->left, (int)rc->top,
                  (int)(rc->right - rc->left), (int)(rc->bottom - rc->top), col);
}

// --- text ---------------------------------------------------------------------------

BOOL CDC::TextOut(int x, int y, LPCTSTR s, int len) {
    if (!m_cgCtx || !s || len <= 0) return TRUE;
    // Advances come from the frozen glyph table, glyph by glyph - never from Core Text - so
    // drawn text lands exactly where the engine's own measurement said it would. Core Text
    // supplies outlines only. See native/render.cpp for the same reasoning.
    //
    // m_yDown says which way "below" runs in this DC's context, which the text path is the
    // only one that has to care about: the baseline sits tmAscent below the cell top, and that
    // is a subtraction in page space and an addition in a window. See NativeDrawPinnedRun.
    NativeDrawPinnedRun((CGContextRef)m_cgCtx, s, len,
                        (int)LX(this, x), (int)LY(this, y), m_textColor,
                        m_yDown != FALSE);
    return TRUE;
}

BOOL CDC::TextOut(int x, int y, const CString& s) {
    return TextOut(x, y, (LPCTSTR)s, s.GetLength());
}
