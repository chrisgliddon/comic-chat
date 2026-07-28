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
        fprintf(stderr, "native: %s with a font the frozen glyph table does not " \
                        "cover. Measuring it would be silently wrong; add the font " \
                        "to CaptureGlyphs in oracle/harness/oracleharness.cpp.\n", \
                (what)); \
        abort(); \
    } while (0)

CFont* CDC::SelectObject(CFont* p) {
    // A null font leaves the DC's notion of "pinned" alone: MFC uses
    // SelectObject(NULL) to mean "no change" in several engine call sites.
    if (p) {
        GlyphTableLoad();
        m_pinnedFont = GlyphFontIsPinned(&p->m_lf) ? TRUE : FALSE;
    }
    return p;
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
    // Only the two the engine actually asks for. The glyph table was captured at
    // 96 dpi (glyphs.json records it), so reporting anything else here would put
    // the advances and the device resolution out of step.
    switch (index) {
        case LOGPIXELSX:
        case LOGPIXELSY:
            return 96;
        default:
            fprintf(stderr, "native: CDC::GetDeviceCaps(%d) is not pinned - the "
                            "glyph table fixes dpi at 96 and nothing else is "
                            "captured.\n", index);
            abort();
    }
}
