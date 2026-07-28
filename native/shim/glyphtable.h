// glyphtable.h - text measurement from the FROZEN glyph table.
//
// RULEBOOK 5 is the contract: text is measured by summing pinned per-character
// advances from oracle/glyphs/glyphs.json, never by asking the platform. Core Text
// would give different answers for font substitution, hinting and shaping, and line
// breaks - and therefore every balloon outline, every panel layout - would diverge
// from the goldens. The table exists precisely so the port does not depend on the
// host's font stack.
//
// The sum model is VERIFIED, not assumed: glyphs.json carries `extentProbes`,
// multi-character strings with their measured GDI widths, and
// sum(advances) == measured width on all 17 including the kerning-prone pairs
// (AV, To, Yo, WA) and the accented characters. tmOverhang is 0, so there is no
// per-string correction either. See the LEDGER entry for why that needed testing
// rather than reasoning: had it failed, the goldens would still have agreed with a
// wrong port, because both sides would have come from the same wrong model.
//
// SCOPE: the table pins exactly one font - Comic Sans MS at 12pt, 96 dpi, MM_TWIPS.
// Measuring any other font from it would be silently wrong, so requests for a font
// that does not match the pinned one ABORT rather than answer. That is deliberate:
// the engine does use other fonts (titles, UI), and aborting names which ones so
// they can be added to the capture. A guess here is indistinguishable from a
// correct answer until a golden fails, which is the failure mode this whole
// approach exists to avoid.

#ifndef NATIVE_SHIM_GLYPHTABLE_H
#define NATIVE_SHIM_GLYPHTABLE_H

#include "win32types.h"

// Loads and caches the table. Returns false if the file is missing or malformed.
// Path defaults to the COMIC_CHAT_GLYPHS environment variable if set, else
// "oracle/glyphs/glyphs.json" relative to the working directory.
//
// An app bundle must ship glyphs.json as a resource and point this at it; the table
// is not optional data, it is part of the engine's definition of correct output.
bool GlyphTableLoad(const char* path = 0);

// True once a table is loaded.
bool GlyphTableReady();

// Advance width in TWIPS for a single byte, or -1 if that byte has no pinned entry.
// The table covers 0x20-0xFF, the whole single-byte MBCS range.
int GlyphAdvance(unsigned char ch);

// Sums advances over len bytes. Aborts if any byte is unpinned, rather than
// skipping it - a silently short string is a wrong line break.
long GlyphTextWidth(const char* s, int len);

// The pinned TEXTMETRIC scalars.
struct GlyphMetrics {
    int lfHeight;
    int tmHeight, tmAscent, tmDescent;
    int tmInternalLeading, tmExternalLeading;
    int tmAveCharWidth, tmMaxCharWidth, tmOverhang;
    // The five CFontInfo values the engine reads (balloon.h:47-56). Pinned rather
    // than recomputed from the metrics, per RULEBOOK 5.
    int m_leading, m_baseAdd, m_lineHeight, m_continuationWidth, m_topOffset;
    const char* faceName;
};
const GlyphMetrics* GlyphTableMetrics();

// Does a LOGFONT match the pinned font? Compared on face name and |lfHeight| only:
// those are what determine the advances, and the engine varies the other fields
// (weight, italic) without the capture covering them - so a match on those two with
// a difference elsewhere is still a request the table cannot honestly answer.
bool GlyphFontIsPinned(const LOGFONT* lf);

#endif // NATIVE_SHIM_GLYPHTABLE_H
