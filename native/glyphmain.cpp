// glyphmain.cpp - checks text measurement through the CDC API.
//
// The table loader self-checks the extent probes, but that only proves the file
// parsed. This goes through the path the ENGINE uses - construct a CDC, select a
// LOGFONT, call GetTextExtent - which additionally covers the pinned-font detection,
// the CString overload, the reported height, and GetTextMetrics.
//
// Why it matters: format.cpp's line breaker measures candidate fragments through
// exactly this call. If the font match were wrong (so m_pinnedFont were FALSE) it
// would abort rather than mismeasure, and if the height were wrong every balloon
// would be the wrong depth. Both are cheap to check here and expensive to diagnose
// from a corpus mismatch.

#include "stdafx.h"
#include "glyphtable.h"
#include <stdio.h>

static int g_fail = 0;

static void expectEq(const char* what, long got, long want) {
    if (got == want) {
        printf("  ok    %-34s %ld\n", what, got);
    } else {
        printf("  FAIL  %-34s got %ld want %ld\n", what, got, want);
        g_fail++;
    }
}

int main(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : 0;
    if (!GlyphTableLoad(path)) {
        fprintf(stderr, "glyphcheck: could not load the glyph table\n");
        return 1;
    }
    const GlyphMetrics* m = GlyphTableMetrics();

    // Build the pinned font exactly as CaptureGlyphs did, from the table's own
    // recorded lfHeight and face - so this cannot drift from what was captured.
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = m->lfHeight;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = ANSI_CHARSET;
    strncpy(lf.lfFaceName, m->faceName, LF_FACESIZE - 1);

    CFont font;
    font.CreateFontIndirect(&lf);

    CDC dc;
    CFont* old = dc.SelectObject(&font);
    (void)old;

    printf("glyphcheck: face '%s' lfHeight %d\n", m->faceName, m->lfHeight);

    // m_continuationWidth is GetTextExtent("...") - a value the engine reads
    // directly, so it doubles as an end-to-end check of the measurement path.
    CSize dots = dc.GetTextExtent("...", 3);
    expectEq("GetTextExtent(\"...\").cx", dots.cx, m->m_continuationWidth);
    expectEq("GetTextExtent(\"...\").cy", dots.cy, m->tmHeight);

    // The CString overload must agree with the pointer+length one.
    CString s("Hello, world!");
    CSize a = dc.GetTextExtent(s);
    CSize b = dc.GetTextExtent("Hello, world!", 13);
    expectEq("CString overload agrees", a.cx, b.cx);

    // Accented characters, the case that was unreproducible before the table was
    // extended to 0x20-0xFF.
    const char kCafe[] = { 'c', 'a', 'f', (char)0xE9, 0 };
    CSize cafe = dc.GetTextExtent(kCafe, 4);
    expectEq("GetTextExtent(\"cafe-acute\")", cafe.cx, 480);

    // Empty and single-space, the degenerate ends of the line breaker's input.
    expectEq("empty string width", dc.GetTextExtent("", 0).cx, 0);
    expectEq("single space width", dc.GetTextExtent(" ", 1).cx, GlyphAdvance(' '));

    TEXTMETRIC tm;
    if (!dc.GetTextMetrics(&tm)) {
        printf("  FAIL  GetTextMetrics returned FALSE\n");
        g_fail++;
    } else {
        expectEq("tm.tmHeight", tm.tmHeight, m->tmHeight);
        expectEq("tm.tmAscent", tm.tmAscent, m->tmAscent);
        expectEq("tm.tmDescent", tm.tmDescent, m->tmDescent);
        expectEq("tm.tmOverhang", tm.tmOverhang, m->tmOverhang);
        expectEq("tm.tmAveCharWidth", tm.tmAveCharWidth, m->tmAveCharWidth);
    }

    expectEq("GetDeviceCaps(LOGPIXELSX)", dc.GetDeviceCaps(LOGPIXELSX), 96);

    // The five CFontInfo scalars, pinned rather than recomputed (RULEBOOK 5).
    // Checked against the values the rulebook quotes, so a re-capture that moved
    // them would be caught here rather than in balloon layout.
    expectEq("cFontInfo.m_leading", m->m_leading, -53);
    expectEq("cFontInfo.m_baseAdd", m->m_baseAdd, 40);
    expectEq("cFontInfo.m_lineHeight", m->m_lineHeight, 292);
    expectEq("cFontInfo.m_continuationWidth", m->m_continuationWidth, 180);
    expectEq("cFontInfo.m_topOffset", m->m_topOffset, 257);

    // A font the table does NOT cover must be detected, not measured. Only the
    // detection is checked - actually measuring would abort by design.
    LOGFONT other = lf;
    strcpy(other.lfFaceName, "Helvetica");
    if (GlyphFontIsPinned(&other)) {
        printf("  FAIL  %-34s an unpinned face was accepted\n", "unpinned font detection");
        g_fail++;
    } else {
        printf("  ok    %-34s rejected 'Helvetica'\n", "unpinned font detection");
    }
    LOGFONT bigger = lf;
    bigger.lfHeight = lf.lfHeight * 2;
    if (GlyphFontIsPinned(&bigger)) {
        printf("  FAIL  %-34s a different size was accepted\n", "unpinned size detection");
        g_fail++;
    } else {
        printf("  ok    %-34s rejected 2x lfHeight\n", "unpinned size detection");
    }

    printf("\nglyphcheck: %s\n", g_fail ? "FAILURES" : "all checks passed");
    return g_fail ? 1 : 0;
}
