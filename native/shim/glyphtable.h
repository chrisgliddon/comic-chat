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
// SCOPE: the table pins a SET of fonts, keyed by (face, lfHeight, italic). fonts.cpp
// creates four per page - balloon, whisper (italic), title and shout - and
// UpdateTitleFonts scales the last two by panel width, so the set is panel-dependent.
//
// A request for a font the table does not contain ABORTS rather than being answered
// from a near neighbour. That is deliberate, and it is how the set was discovered: the
// corpus replay aborted naming lfHeight -576, the title font, which the original
// single-font table had no entry for. A guess would have been indistinguishable from a
// correct answer until a golden failed several layers downstream.

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

// Selects the active font by (face, |lfHeight|, italic). Returns false if the table has
// no such entry; the caller is expected to refuse to measure rather than fall back.
bool GlyphSelectFont(const LOGFONT* lf);

// Selects the "stock" entry - the font a fresh DC holds before anything is selected.
// This is what restoring a NULL previous font must mean, and what WidestWord measures
// with. Returns false if the capture has no stock entry.
bool GlyphSelectStock();

// Advance width in TWIPS for a single byte in the ACTIVE font, or -1 if unpinned.
// Each font covers 0x00-0xFF. The control range below 0x20 is included because
// CLabel::WidestWord measures one byte past the end of the string (balloon.cpp:738,
// `szEnd - szStart + 1`), so GDI measures the NUL terminator as a glyph and the goldens
// encode its advance. A table starting at 0x20 cannot reproduce that.
int GlyphAdvance(unsigned char ch);

// CP-1252 byte -> Unicode scalar, for DRAWING only. Measurement never needs it (the table
// is indexed by byte), but Core Text does: the 0x80-0x9F block holds the smart quotes,
// dashes and euro, and treating it as Latin-1 renders them as control characters.
unsigned short GlyphCp1252ToUnicode(unsigned char ch);

// Sums advances over len bytes in the active font. Aborts if any byte is unpinned,
// rather than skipping it - a silently short string is a wrong line break.
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
    // tmCharSet matters: fonts.cpp italicises the whisper font only when it is < 3 or
    // one of the European sets, so a wrong value italicises the wrong balloons.
    int tmCharSet;
    int lfItalic;
    const char* faceName;
};

// Metrics for the ACTIVE font (whatever GlyphSelectFont last accepted).
const GlyphMetrics* GlyphTableMetrics();

// How many fonts the table holds, and a description of one - for diagnostics when a
// lookup fails, so the message can list what IS available.
int GlyphFontCount();
const GlyphMetrics* GlyphFontAt(int i);

// Is there an entry for this LOGFONT? Matched on face name, |lfHeight| and italic -
// the three things that change the advances. Other fields (weight, underline) are not
// varied by the engine and are not captured, so they are not compared.
bool GlyphFontIsPinned(const LOGFONT* lf);

#endif // NATIVE_SHIM_GLYPHTABLE_H
