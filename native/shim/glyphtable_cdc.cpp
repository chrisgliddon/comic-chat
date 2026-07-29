// glyphtable_cdc.cpp - CDC's measurement methods, backed by the frozen glyph table.
//
// Separated from gdishim.h so that header stays free of the table's dependencies
// (ojson, file I/O). See glyphtable.h for the contract; the short version is that
// measuring with Core Text instead would move every line break and therefore every
// balloon and panel, so the pinned advances are the only permitted source.

#include "stdafx.h"
#include "glyphtable.h"

// Fails loudly rather than substituting the platform's answer. Reaching this means
// a font was selected that the table was not captured for - which is information
// worth having, since it names the font that needs adding to CaptureGlyphs.
#define GLYPH_WRONG_FONT(what) \
    do { \
        fprintf(stderr, "native: %s with a font the frozen glyph table does not cover.\n" \
                        "  Measuring it would be silently wrong. Add the font to\n" \
                        "  CaptureGlyphs in oracle/harness/oracleharness.cpp.\n" \
                        "  The table currently holds:\n", (what)); \
        for (int _i = 0; _i < GlyphFontCount(); _i++) { \
            const GlyphMetrics* _m = GlyphFontAt(_i); \
            fprintf(stderr, "    face='%s' lfHeight=%d italic=%d\n", \
                    _m->faceName, _m->lfHeight, _m->lfItalic); \
        } \
        abort(); \
    } while (0)

// Stands for "the font the DC already had". MFC's SelectObject returns
// CFont::FromHandle(hOldFont) - a temporary wrapper around the outgoing GDI font - and it
// is NEVER null for a valid DC, because a DC always has some font selected. Handing back a
// real object matters because callers TEST it:
//
//     CFont* pOldFont = pDc->SelectObject(m_fontBalloon);   // fonts.cpp:61
//     ...
//     if (pOldFont)                                         // fonts.cpp:69
//         pDc->SelectObject(pOldFont);
//
// Returning NULL for "nothing selected yet" made that restore be SKIPPED, so the DC kept
// Comic Sans selected for the rest of the run. CLabel::WidestWord - which selects no font
// of its own - then measured in Comic Sans (3885) where Windows measured in the DC's stock
// System font (3180), and that one number widened every balloon in 14 of 15 corpus cases.
static CFont g_stockFontSentinel;

CFont* CDC::SelectObject(CFont* p) {
    GlyphTableLoad();

    // The PREVIOUSLY selected font - GDI's contract, and the whole mechanism behind the
    // save/restore idiom. This used to `return p`, which made every restore re-select the
    // font being saved.
    CFont* prev = m_pCurFont ? m_pCurFont : &g_stockFontSentinel;

    if (!p || p == &g_stockFontSentinel) {
        // Back to the DC's original font. m_pCurFont returns to 0 so the next save hands
        // back the sentinel again. If the capture has no stock entry, the previous
        // behaviour is kept rather than aborting, so an older glyphs.json still loads.
        m_pCurFont = 0;
        if (GlyphSelectStock()) m_pinnedFont = TRUE;
    } else {
        // Selects the matching entry, so subsequent measurement uses THIS font's advances.
        // The engine switches between balloon, whisper (italic), title and shout repeatedly
        // while laying out a page and each has different widths - measuring a title with
        // balloon advances would misplace every title.
        m_pCurFont = p;
        m_pinnedFont = GlyphSelectFont(&p->m_lf) ? TRUE : FALSE;
    }
    return prev;
}

SIZE CDC::GetTextExtent(LPCTSTR s, int len) const {
    SIZE sz;
    sz.cx = 0;
    sz.cy = 0;
    if (!GlyphTableReady() && !GlyphTableLoad()) {
        fprintf(stderr, "native: GetTextExtent before the glyph table loaded\n");
        abort();
    }
    if (!m_pinnedFont) GLYPH_WRONG_FONT("CDC::GetTextExtent");
    sz.cx = (LONG)GlyphTextWidth(s, len);
    // Height is the pinned tmHeight for every string, matching GDI: GetTextExtent
    // reports the font's cell height, not the ink extent of these particular
    // characters. The extentProbes confirm it - every probe's height is 345
    // regardless of content.
    sz.cy = (LONG)GlyphTableMetrics()->tmHeight;
    return sz;
}

SIZE CDC::GetTextExtent(const CString& s) const {
    return GetTextExtent((LPCTSTR)s, s.GetLength());
}

BOOL CDC::GetTextMetrics(TEXTMETRIC* tm) const {
    if (!tm) return FALSE;
    if (!GlyphTableReady() && !GlyphTableLoad()) return FALSE;
    if (!m_pinnedFont) GLYPH_WRONG_FONT("CDC::GetTextMetrics");
    const GlyphMetrics* m = GlyphTableMetrics();
    memset(tm, 0, sizeof(*tm));
    tm->tmHeight          = m->tmHeight;
    tm->tmAscent          = m->tmAscent;
    tm->tmDescent         = m->tmDescent;
    tm->tmInternalLeading = m->tmInternalLeading;
    tm->tmExternalLeading = m->tmExternalLeading;
    tm->tmAveCharWidth    = m->tmAveCharWidth;
    tm->tmMaxCharWidth    = m->tmMaxCharWidth;
    tm->tmOverhang        = m->tmOverhang;
    // The remaining fields are not pinned because the engine does not read them.
    // Left zeroed rather than invented; if a port ever needs one, capture it.
    return TRUE;
}

BOOL CDC::GetCharWidth(UINT first, UINT last, int* buf) const {
    if (!buf || last < first) return FALSE;
    if (!GlyphTableReady() && !GlyphTableLoad()) return FALSE;
    if (!m_pinnedFont) GLYPH_WRONG_FONT("CDC::GetCharWidth");
    for (UINT c = first; c <= last; c++) {
        int a = (c <= 0xFF) ? GlyphAdvance((unsigned char)c) : -1;
        if (a < 0) {
            fprintf(stderr, "native: GetCharWidth: no pinned advance for 0x%02X\n", c);
            abort();
        }
        buf[c - first] = a;
    }
    return TRUE;
}

int CDC::GetDeviceCaps(int index) const {
    switch (index) {
        // The glyph table was captured at 96 dpi (glyphs.json records it), so reporting
        // anything else here would put the advances and the device resolution out of step.
        case LOGPIXELSX:
        case LOGPIXELSY:
            return 96;

        // The display's colour format, which bodycam.cpp:969 reads when choosing the DIB
        // section format for its retained self-view bitmap. These are FACTS about a macOS
        // display rather than pins, which is why they can be answered rather than refused:
        // 32 bits per pixel, one plane, and no hardware palette. RC_PALETTE clear is the
        // load-bearing part - it sends CBodyCam down its non-palettised branch, which is
        // correct, because there is no palette to realise here. Claiming otherwise would
        // have it build an 8-bit DIB against gpLogPal and index into a palette that does
        // not exist.
        // 24, not 32, and the difference is load-bearing rather than cosmetic. macOS displays
        // are 8 bits per colour component either way, so both answers are defensible as a
        // colour depth - but CreateRetainedBitmap (bodycam.cpp:993) branches on this value:
        //
        //   24 or 1  -> build a DIB section in that depth directly. Clean.
        //   anything else -> GetOptimalDibSectionInfo, which PROBES the display's channel
        //                    layout with ::GetDIBits against a real HBITMAP. There is no
        //                    such bitmap here, that probe returns 0 scanlines, and the
        //                    header is left with biBitCount 0 - an unusable DIB section.
        //
        // So 24 is the answer that is both true about the colour depth and lands on the path
        // that can actually be served. It is also the depth cgblit's 24-bit BGR case already
        // reads, which is what the offscreen surface hands back.
        case BITSPIXEL: return 24;
        case PLANES:    return 1;
        case RASTERCAPS: return 0;      // specifically NOT RC_PALETTE
        case NUMCOLORS: return -1;      // GDI's answer for a non-palettised device
        default:
            fprintf(stderr, "native: CDC::GetDeviceCaps(%d) is not pinned - the "
                            "glyph table fixes dpi at 96 and nothing else is "
                            "captured.\n", index);
            abort();
    }
}

// GetTextFace - the PHYSICAL face name GDI resolved the LOGFONT to. Answered from the
// frozen table, which records faceName per captured font.
//
// This is not a courtesy: fonts.cpp:83 and :125 compute
//     doVKern = (strcmp(szPhysFaceName, "Comic Sans MS") == 0) ? 1 : 0;
// and then pass (int)(-40 * reduction * doVKern) as CFontInfo's nLeading. With an empty
// name doVKern was 0, so the balloon font's m_leading was 0 rather than -53 and its
// m_lineHeight 345 rather than the pinned 292. That fed AreaEstimate -> GetCloudEstimate
// -> balloon width, so every balloon in the corpus came out wider, wrapped to fewer lines,
// and produced a different spline and trajectory.
//
// Aborting when no font is selected matches the rest of this file: silently returning an
// empty name is what caused the bug in the first place.
int CDC::GetTextFace(int n, LPTSTR buf) const {
    if (!GlyphTableReady() && !GlyphTableLoad()) {
        fprintf(stderr, "native: CDC::GetTextFace before the glyph table loaded\n");
        abort();
    }
    if (!m_pinnedFont) GLYPH_WRONG_FONT("CDC::GetTextFace");
    const char* face = GlyphTableMetrics()->faceName;
    if (!face) face = "";
    if (!buf || n <= 0) return 0;
    int len = (int)strlen(face);
    if (len > n - 1) len = n - 1;
    memcpy(buf, face, (size_t)len);
    buf[len] = '\0';
    return len;
}

int CDC::GetTextFace(CString& s) const {
    char buf[LF_FACESIZE];
    int n = GetTextFace(LF_FACESIZE, buf);
    s = buf;
    return n;
}
