// glyphtable.cpp - see glyphtable.h for the contract and why measurement must not
// touch the platform's font stack.

#include "stdafx.h"
#include "glyphtable.h"
#include "../../oracle/harness/ojson.h"

#include <stdio.h>
#include <string.h>
#include <string>

namespace {

bool g_loaded = false;
// -1 marks "no pinned entry". Indexed by byte, so 0x00-0x1F stay -1: the table
// covers 0x20-0xFF and control bytes genuinely have no pinned width.
int g_advance[256];
GlyphMetrics g_metrics;
std::string g_faceName;

int GetIntField(const ojson::Value& o, const char* key, int def) {
    return (int)o.GetInt(key, def);
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
    const ojson::Value* font = root.Find("font");
    if (!font) {
        fprintf(stderr, "glyphtable: no 'font' object in %s\n", p);
        return false;
    }

    for (int i = 0; i < 256; i++) g_advance[i] = -1;

    const ojson::Value* adv = font->Find("glyphAdvances");
    if (!adv || adv->type != ojson::T_ARRAY) {
        fprintf(stderr, "glyphtable: no glyphAdvances array in %s\n", p);
        return false;
    }
    int n = 0;
    for (size_t i = 0; i < adv->arr.size(); i++) {
        const ojson::Value& e = adv->arr[i];
        long c = e.GetInt("char", -1);
        long w = e.GetInt("advance", -1);
        if (c < 0 || c > 255 || w < 0) continue;
        g_advance[c] = (int)w;
        n++;
    }

    memset(&g_metrics, 0, sizeof(g_metrics));
    g_faceName = font->GetStr("faceName", "");
    g_metrics.faceName = g_faceName.c_str();
    g_metrics.lfHeight          = GetIntField(*font, "lfHeight", 0);
    g_metrics.tmHeight          = GetIntField(*font, "tmHeight", 0);
    g_metrics.tmAscent          = GetIntField(*font, "tmAscent", 0);
    g_metrics.tmDescent         = GetIntField(*font, "tmDescent", 0);
    g_metrics.tmInternalLeading = GetIntField(*font, "tmInternalLeading", 0);
    g_metrics.tmExternalLeading = GetIntField(*font, "tmExternalLeading", 0);
    g_metrics.tmAveCharWidth    = GetIntField(*font, "tmAveCharWidth", 0);
    g_metrics.tmMaxCharWidth    = GetIntField(*font, "tmMaxCharWidth", 0);
    g_metrics.tmOverhang        = GetIntField(*font, "tmOverhang", 0);

    const ojson::Value* cfi = font->Find("cFontInfo");
    if (cfi) {
        g_metrics.m_leading           = GetIntField(*cfi, "m_leading", 0);
        g_metrics.m_baseAdd           = GetIntField(*cfi, "m_baseAdd", 0);
        g_metrics.m_lineHeight        = GetIntField(*cfi, "m_lineHeight", 0);
        g_metrics.m_continuationWidth = GetIntField(*cfi, "m_continuationWidth", 0);
        g_metrics.m_topOffset         = GetIntField(*cfi, "m_topOffset", 0);
    }

    // Self-check against the frozen extent probes. This is cheap and it validates
    // the loader AND the sum model on every startup: if a future table were captured
    // from a font that kerns, or the loader mis-parsed an advance, this catches it
    // here rather than as a mysterious layout difference much later.
    const ojson::Value* probes = font->Find("extentProbes");
    int probeFail = 0;
    if (probes && probes->type == ojson::T_ARRAY) {
        for (size_t i = 0; i < probes->arr.size(); i++) {
            const ojson::Value& e = probes->arr[i];
            std::string s = e.GetStr("text", "");
            long expect = e.GetInt("width", -1);
            long sum = 0;
            bool unpinned = false;
            for (size_t k = 0; k < s.size(); k++) {
                int a = g_advance[(unsigned char)s[k]];
                if (a < 0) { unpinned = true; break; }
                sum += a;
            }
            if (unpinned || sum != expect) {
                fprintf(stderr, "glyphtable: extent probe %d disagrees "
                                "(sum=%ld expected=%ld)\n", (int)i, sum, expect);
                probeFail++;
            }
        }
    }
    if (probeFail) {
        fprintf(stderr, "glyphtable: %d extent probe(s) failed - the sum-of-advances "
                        "model does not hold for this table, so measurement would be "
                        "wrong. Refusing to load.\n", probeFail);
        return false;
    }

    g_loaded = true;
    fprintf(stderr, "glyphtable: %s - %d advances, face '%s', tmHeight %d, %d probes ok\n",
            p, n, g_faceName.c_str(), g_metrics.tmHeight,
            probes ? (int)probes->arr.size() : 0);
    return true;
}

bool GlyphTableReady() { return g_loaded; }

const GlyphMetrics* GlyphTableMetrics() { return g_loaded ? &g_metrics : 0; }

int GlyphAdvance(unsigned char ch) {
    if (!g_loaded) return -1;
    return g_advance[ch];
}

long GlyphTextWidth(const char* s, int len) {
    if (!g_loaded) {
        fprintf(stderr, "glyphtable: measurement requested before the table was "
                        "loaded - call GlyphTableLoad() during init\n");
        abort();
    }
    if (!s || len <= 0) return 0;
    long sum = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        int a = g_advance[c];
        if (a < 0) {
            // Skipping would silently shorten the string and move the line break.
            // The table covers 0x20-0xFF, so this means either a control byte or a
            // genuinely multi-byte sequence (the deferred Tier-1 #13 CJK question).
            fprintf(stderr, "glyphtable: no pinned advance for byte 0x%02X - cannot "
                            "measure honestly. Extend the capture in "
                            "oracle/harness/oracleharness.cpp CaptureGlyphs.\n", c);
            abort();
        }
        sum += a;
    }
    return sum;
}

bool GlyphFontIsPinned(const LOGFONT* lf) {
    if (!g_loaded || !lf) return false;
    if (g_faceName.empty()) return false;
    if (strcasecmp(lf->lfFaceName, g_faceName.c_str()) != 0) return false;
    long a = lf->lfHeight < 0 ? -(long)lf->lfHeight : (long)lf->lfHeight;
    long b = g_metrics.lfHeight < 0 ? -(long)g_metrics.lfHeight : (long)g_metrics.lfHeight;
    return a == b;
}
