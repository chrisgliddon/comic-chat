// glyphtable.cpp - see glyphtable.h for the contract and why measurement must not
// touch the platform's font stack.

#include "stdafx.h"
#include "glyphtable.h"
#include "../../oracle/harness/ojson.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

// One captured font. The advance array is per-font because italic and each size have
// genuinely different widths - sharing one array across sizes was the original bug.
struct FontEntry {
    GlyphMetrics m;
    std::string  face;
    std::string  role;
    int          advance[256];   // -1 == no pinned entry
};

bool g_loaded = false;
std::vector<FontEntry> g_fonts;
int g_active = -1;               // index into g_fonts, or -1 for none selected

int GetIntField(const ojson::Value& o, const char* key, int def) {
    return (int)o.GetInt(key, def);
}

// Reads one font object (either the legacy top-level `font` or an element of `fonts`).
bool ReadFont(const ojson::Value& f, FontEntry& out, std::string& err) {
    memset(&out.m, 0, sizeof(out.m));
    for (int i = 0; i < 256; i++) out.advance[i] = -1;

    out.face = f.GetStr("faceName", "");
    out.role = f.GetStr("role", "");
    if (out.face.empty()) { err = "font entry has no faceName"; return false; }
    out.m.faceName = out.face.c_str();

    out.m.lfHeight          = GetIntField(f, "lfHeight", 0);
    out.m.lfItalic          = GetIntField(f, "lfItalic", 0);
    out.m.tmHeight          = GetIntField(f, "tmHeight", 0);
    out.m.tmAscent          = GetIntField(f, "tmAscent", 0);
    out.m.tmDescent         = GetIntField(f, "tmDescent", 0);
    out.m.tmInternalLeading = GetIntField(f, "tmInternalLeading", 0);
    out.m.tmExternalLeading = GetIntField(f, "tmExternalLeading", 0);
    out.m.tmAveCharWidth    = GetIntField(f, "tmAveCharWidth", 0);
    out.m.tmMaxCharWidth    = GetIntField(f, "tmMaxCharWidth", 0);
    out.m.tmOverhang        = GetIntField(f, "tmOverhang", 0);
    out.m.tmCharSet         = GetIntField(f, "tmCharSet", 0);

    const ojson::Value* cfi = f.Find("cFontInfo");
    if (cfi) {
        out.m.m_leading           = GetIntField(*cfi, "m_leading", 0);
        out.m.m_baseAdd           = GetIntField(*cfi, "m_baseAdd", 0);
        out.m.m_lineHeight        = GetIntField(*cfi, "m_lineHeight", 0);
        out.m.m_continuationWidth = GetIntField(*cfi, "m_continuationWidth", 0);
        out.m.m_topOffset         = GetIntField(*cfi, "m_topOffset", 0);
    }

    const ojson::Value* adv = f.Find("glyphAdvances");
    if (!adv || adv->type != ojson::T_ARRAY) { err = "font entry has no glyphAdvances"; return false; }
    for (size_t i = 0; i < adv->arr.size(); i++) {
        long c = adv->arr[i].GetInt("char", -1);
        long w = adv->arr[i].GetInt("advance", -1);
        if (c < 0 || c > 255 || w < 0) continue;
        out.advance[c] = (int)w;
    }

    // Per-font self-check of the sum-of-advances model. Run for EVERY font, not just
    // the balloon one: italic and the title sizes are separate captures and could each
    // fail independently, and a silently wrong title width misplaces every panel title.
    const ojson::Value* probes = f.Find("extentProbes");
    if (probes && probes->type == ojson::T_ARRAY) {
        for (size_t i = 0; i < probes->arr.size(); i++) {
            std::string s = probes->arr[i].GetStr("text", "");
            long expect = probes->arr[i].GetInt("width", -1);
            long sum = 0;
            bool unpinned = false;
            for (size_t k = 0; k < s.size(); k++) {
                int a = out.advance[(unsigned char)s[k]];
                if (a < 0) { unpinned = true; break; }
                sum += a;
            }
            if (unpinned || sum != expect) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "extent probe %d disagrees for font lfHeight=%d italic=%d "
                         "(sum=%ld expected=%ld) - the sum-of-advances model does not "
                         "hold for this capture",
                         (int)i, out.m.lfHeight, out.m.lfItalic, sum, expect);
                err = buf;
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool GlyphTableLoad(const char* path) {
    if (g_loaded) return true;

    const char* p = path;
    if (!p) p = getenv("COMIC_CHAT_GLYPHS");
    if (!p) p = "oracle/glyphs/glyphs.json";

    FILE* f = fopen(p, "rb");
    if (!f) {
        fprintf(stderr, "glyphtable: cannot open %s\n", p);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string text((size_t)(sz > 0 ? sz : 0), '\0');
    if (sz > 0 && fread(&text[0], 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        fprintf(stderr, "glyphtable: short read on %s\n", p);
        return false;
    }
    fclose(f);

    ojson::Value root;
    std::string err;
    if (!ojson::Parse(text, root, err)) {
        fprintf(stderr, "glyphtable: parse error in %s: %s\n", p, err.c_str());
        return false;
    }

    g_fonts.clear();

    // Prefer the `fonts` array; fall back to the legacy single `font` object so an older
    // glyphs.json still loads (with only the balloon font, which will then abort on the
    // first title measurement - correctly, and with a message naming the size).
    const ojson::Value* fonts = root.Find("fonts");
    if (fonts && fonts->type == ojson::T_ARRAY && !fonts->arr.empty()) {
        for (size_t i = 0; i < fonts->arr.size(); i++) {
            FontEntry e;
            if (!ReadFont(fonts->arr[i], e, err)) {
                fprintf(stderr, "glyphtable: %s (entry %d of 'fonts')\n", err.c_str(), (int)i);
                return false;
            }
            g_fonts.push_back(e);
        }
    } else {
        const ojson::Value* one = root.Find("font");
        if (!one) {
            fprintf(stderr, "glyphtable: %s has neither a 'fonts' array nor a 'font' object\n", p);
            return false;
        }
        FontEntry e;
        if (!ReadFont(*one, e, err)) {
            fprintf(stderr, "glyphtable: %s\n", err.c_str());
            return false;
        }
        g_fonts.push_back(e);
        fprintf(stderr, "glyphtable: %s has only the legacy single-font shape; title and\n"
                        "  shout measurements will abort. Re-capture with --glyphs.\n", p);
    }

    // The cFontInfo scalars live ONLY on the legacy top-level `font` object: the
    // multi-font capture emits per-font metrics and advances, but CFontInfo is derived
    // once for the balloon font and was never added to the `fonts` elements. So when the
    // array branch is taken above, nothing has read them yet and every entry's
    // m_lineHeight is 0 - which silently zeroed all five, and glyphmain caught it.
    //
    // Reading them from the legacy object is correct rather than a patch: that object IS
    // the balloon font (same face, same lfHeight -240), so it is the same source the
    // capture would write into fonts[0]. Doing it here also avoids re-freezing the table,
    // which would cost a Windows CI run to regenerate data we already hold.
    if (!g_fonts.empty() && g_fonts[0].m.m_lineHeight == 0) {
        const ojson::Value* legacy = root.Find("font");
        if (legacy) {
            FontEntry le;
            std::string lerr;
            if (ReadFont(*legacy, le, lerr) && le.m.m_lineHeight != 0) {
                // Matched on font identity (face, lfHeight, italic), NOT on role: the
                // legacy object carries `role: null`, so comparing roles matched nothing
                // and left the scalars zeroed. Identity is also the right key - it is
                // what FindFont uses, and it is what determines the advances.
                for (size_t i = 0; i < g_fonts.size(); i++) {
                    if (g_fonts[i].face == le.face
                        && g_fonts[i].m.lfHeight == le.m.lfHeight
                        && !g_fonts[i].m.lfItalic == !le.m.lfItalic) {
                        g_fonts[i].m.m_leading           = le.m.m_leading;
                        g_fonts[i].m.m_baseAdd           = le.m.m_baseAdd;
                        g_fonts[i].m.m_lineHeight        = le.m.m_lineHeight;
                        g_fonts[i].m.m_continuationWidth = le.m.m_continuationWidth;
                        g_fonts[i].m.m_topOffset         = le.m.m_topOffset;
                    }
                }
            }
        }
    }

    // The cFontInfo scalars are only captured on the balloon entry (the engine derives
    // the other CFontInfos with different leading/baseAdd), so copy them across for
    // callers that ask the active font. They are a property of the balloon font.
    const FontEntry* balloon = 0;
    for (size_t i = 0; i < g_fonts.size(); i++) {
        if (g_fonts[i].m.m_lineHeight != 0) { balloon = &g_fonts[i]; break; }
    }
    if (balloon) {
        for (size_t i = 0; i < g_fonts.size(); i++) {
            if (g_fonts[i].m.m_lineHeight == 0) {
                g_fonts[i].m.m_leading           = balloon->m.m_leading;
                g_fonts[i].m.m_baseAdd           = balloon->m.m_baseAdd;
                g_fonts[i].m.m_lineHeight        = balloon->m.m_lineHeight;
                g_fonts[i].m.m_continuationWidth = balloon->m.m_continuationWidth;
                g_fonts[i].m.m_topOffset         = balloon->m.m_topOffset;
            }
        }
    }

    // Default active font: the balloon one, so a caller that measures before selecting
    // gets the size the engine uses most.
    g_active = 0;
    for (size_t i = 0; i < g_fonts.size(); i++) {
        if (g_fonts[i].role == "balloon") { g_active = (int)i; break; }
    }

    g_loaded = true;
    fprintf(stderr, "glyphtable: %s - %d font(s):", p, (int)g_fonts.size());
    for (size_t i = 0; i < g_fonts.size(); i++) {
        fprintf(stderr, " [%s h=%d%s]", g_fonts[i].role.c_str(), g_fonts[i].m.lfHeight,
                g_fonts[i].m.lfItalic ? " italic" : "");
    }
    fprintf(stderr, "\n");
    return true;
}

bool GlyphTableReady() { return g_loaded; }

int GlyphFontCount() { return (int)g_fonts.size(); }

const GlyphMetrics* GlyphFontAt(int i) {
    return (i >= 0 && i < (int)g_fonts.size()) ? &g_fonts[(size_t)i].m : 0;
}

const GlyphMetrics* GlyphTableMetrics() {
    return (g_loaded && g_active >= 0) ? &g_fonts[(size_t)g_active].m : 0;
}

// Matched on face, |lfHeight| and italic - the three things that change the advances.
static int FindFont(const LOGFONT* lf) {
    if (!g_loaded || !lf) return -1;
    long want = lf->lfHeight < 0 ? -(long)lf->lfHeight : (long)lf->lfHeight;
    int wantItalic = lf->lfItalic ? 1 : 0;
    for (size_t i = 0; i < g_fonts.size(); i++) {
        const FontEntry& e = g_fonts[i];
        long h = e.m.lfHeight < 0 ? -(long)e.m.lfHeight : (long)e.m.lfHeight;
        if (h != want) continue;
        if ((e.m.lfItalic ? 1 : 0) != wantItalic) continue;
        if (strcasecmp(lf->lfFaceName, e.face.c_str()) != 0) continue;
        return (int)i;
    }
    return -1;
}

bool GlyphFontIsPinned(const LOGFONT* lf) { return FindFont(lf) >= 0; }

bool GlyphSelectFont(const LOGFONT* lf) {
    int i = FindFont(lf);
    if (i < 0) return false;
    g_active = i;
    return true;
}

int GlyphAdvance(unsigned char ch) {
    if (!g_loaded || g_active < 0) return -1;
    return g_fonts[(size_t)g_active].advance[ch];
}

long GlyphTextWidth(const char* s, int len) {
    if (!g_loaded || g_active < 0) {
        fprintf(stderr, "glyphtable: measurement requested with no font selected - call\n"
                        "  GlyphTableLoad() during init and select a font before measuring\n");
        abort();
    }
    if (!s || len <= 0) return 0;
    const FontEntry& e = g_fonts[(size_t)g_active];
    long sum = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        int a = e.advance[c];
        if (a < 0) {
            // Skipping would silently shorten the string and move the line break. Each
            // font covers 0x20-0xFF, so this is either a control byte or a genuinely
            // multi-byte sequence (the deferred Tier-1 #13 CJK question).
            fprintf(stderr, "glyphtable: no pinned advance for byte 0x%02X in font "
                            "lfHeight=%d italic=%d - cannot measure honestly. Extend\n"
                            "  CaptureGlyphs in oracle/harness/oracleharness.cpp.\n",
                    c, e.m.lfHeight, e.m.lfItalic);
            abort();
        }
        sum += a;
    }
    return sum;
}
