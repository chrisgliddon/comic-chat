// stringtable.cpp - see stringtable.h.

#include "stdafx.h"
#include "stringtable.h"
#include "respath.h"
#include "../../oracle/harness/ojson.h"

#include <stdio.h>
#include <map>
#include <string>

namespace {
bool g_loaded = false;
bool g_tried = false;
std::map<UINT, std::string> g_strings;
}

bool StringTableLoad(const char* path) {
    if (g_loaded) return true;
    // Only attempt once: LoadString is called in loops, and retrying a missing file per
    // call would emit thousands of identical warnings.
    if (g_tried) return false;
    g_tried = true;

    // See respath.h for the search order.
    std::string chosen, tried;
    FILE* f;
    if (path) {
        f = fopen(path, "rb");
        chosen = path;
        tried = path;
    } else {
        f = NativeResourceOpen("COMIC_CHAT_STRINGS", "native/resources/strings.json", chosen, tried);
    }
    if (!f) {
        fprintf(stderr, "stringtable: cannot open the string table - the engine will have no\n"
                        "  emotion rules and no UI strings. Generate it with native/gen-strings.py.\n"
                        "  Tried: %s\n", tried.c_str());
        return false;
    }
    const char* p = chosen.c_str();
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string text((size_t)(sz > 0 ? sz : 0), '\0');
    if (sz > 0 && fread(&text[0], 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        fprintf(stderr, "stringtable: short read on %s\n", p);
        return false;
    }
    fclose(f);

    ojson::Value root;
    std::string err;
    if (!ojson::Parse(text, root, err)) {
        fprintf(stderr, "stringtable: parse error in %s: %s\n", p, err.c_str());
        return false;
    }
    const ojson::Value* strs = root.Find("strings");
    if (!strs || strs->type != ojson::T_OBJECT) {
        fprintf(stderr, "stringtable: no 'strings' object in %s\n", p);
        return false;
    }
    for (size_t i = 0; i < strs->obj.size(); i++) {
        const std::string& key = strs->obj[i].first;
        const ojson::Value& v = strs->obj[i].second;
        if (v.type != ojson::T_STRING) continue;
        g_strings[(UINT)strtoul(key.c_str(), 0, 10)] = v.s;
    }

    g_loaded = true;
    fprintf(stderr, "stringtable: %s - %d strings\n", p, (int)g_strings.size());
    return true;
}

const char* StringTableLookup(UINT id) {
    if (!g_loaded && !StringTableLoad()) return 0;
    std::map<UINT, std::string>::const_iterator it = g_strings.find(id);
    return (it == g_strings.end()) ? 0 : it->second.c_str();
}

int StringTableCount() { return (int)g_strings.size(); }

// CString::LoadString, defined here rather than inline in mfcshim.h because that header
// must not depend on ojson or file I/O.
BOOL CString::LoadString(UINT id) {
    const char* s = StringTableLookup(id);
    if (!s) { Empty(); return FALSE; }
    *this = s;
    return TRUE;
}
