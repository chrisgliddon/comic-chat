// resources.cpp - see resources.h.

#include "stdafx.h"
#include "resources.h"
#include "respath.h"
#include "../../oracle/harness/ojson.h"

#include <stdio.h>
#include <map>
#include <string>
#include <vector>

namespace {

struct Entry {
    std::string type;           // "DIB", "BITMAP", "ICON"
    std::string file;           // forward-slashed, relative to the root
    std::string name;           // the RC symbol, for diagnostics
    std::vector<BYTE> bytes;    // the file, read on first use and kept
    bool tried;                 // so a missing file is reported once, not per call
    Entry() : tried(false) {}
};

bool g_loaded = false;
bool g_tried = false;
std::string g_root;

// Keyed "<id>:<type>", matching the manifest: one id can carry two resources of different
// types. IDR_MAINFRAME is both the application ICON and the toolbar BITMAP.
std::map<std::string, Entry> g_entries;

std::string KeyOf(UINT id, const char* type) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u:", id);
    return std::string(buf) + (type ? type : "");
}

// Reads the whole file into e.bytes. Kept resident, the way a locked resource would be:
// LockResource hands out an interior pointer and dib.cpp is explicitly written to not free
// it ("not required to unlock or free the resource in Win32").
bool EnsureBytes(Entry& e) {
    if (!e.bytes.empty()) return true;
    if (e.tried) return false;
    e.tried = true;

    std::string path = g_root.empty() ? e.file : (g_root + "/" + e.file);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "resources: %s (%s) not found at %s\n",
                e.name.c_str(), e.type.c_str(), path.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return false; }
    e.bytes.resize((size_t)sz);
    size_t got = fread(&e.bytes[0], 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        fprintf(stderr, "resources: short read on %s\n", path.c_str());
        e.bytes.clear();
        return false;
    }
    return true;
}

} // namespace

void ResourceSetRoot(const char* dir) { if (dir) g_root = dir; }

bool ResourceLoadManifest() {
    if (g_loaded) return true;
    if (g_tried) return false;
    g_tried = true;

    std::string chosen, tried;
    FILE* f = NativeResourceOpen("COMIC_CHAT_BITMAPS", "native/resources/bitmaps.json",
                                 chosen, tried);
    if (!f) {
        // Not fatal: every FindResource then returns NULL, which is what it did before this
        // file existed. The engine's own callers check for that - CDIB::Load(WORD) returns
        // FALSE - so the failure mode is a blank icon rather than a crash. Except in
        // CBodyCamIcons::GetIcon, which is why the message says what breaks.
        fprintf(stderr, "resources: cannot open bitmaps.json - the emotion wheel's face\n"
                        "  icons and the toolbar strips will be unavailable. Generate it\n"
                        "  with native/gen-bitmaps.py. Tried: %s\n", tried.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string text((size_t)(sz > 0 ? sz : 0), '\0');
    if (sz > 0 && fread(&text[0], 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        fprintf(stderr, "resources: short read on %s\n", chosen.c_str());
        return false;
    }
    fclose(f);

    ojson::Value root;
    std::string err;
    if (!ojson::Parse(text, root, err)) {
        fprintf(stderr, "resources: parse error in %s: %s\n", chosen.c_str(), err.c_str());
        return false;
    }
    if (root.type != ojson::T_OBJECT) {
        fprintf(stderr, "resources: %s is not an object\n", chosen.c_str());
        return false;
    }
    for (size_t i = 0; i < root.obj.size(); i++) {
        const std::string& key = root.obj[i].first;
        const ojson::Value& v = root.obj[i].second;
        if (v.type != ojson::T_OBJECT) continue;
        const ojson::Value* t = v.Find("type");
        const ojson::Value* fi = v.Find("file");
        const ojson::Value* nm = v.Find("name");
        if (!t || !fi) continue;
        Entry e;
        e.type = t->s;
        e.file = fi->s;
        e.name = nm ? nm->s : key;
        g_entries[key] = e;
    }

    g_loaded = true;
    // The root matters enough to name: a manifest that loads against the wrong root finds
    // nothing, and the per-file messages would otherwise be the first hint.
    fprintf(stderr, "resources: %s - %d resources, root=%s\n", chosen.c_str(),
            (int)g_entries.size(), g_root.empty() ? "(cwd)" : g_root.c_str());
    return true;
}

int ResourceManifestCount() { return (int)g_entries.size(); }

// --- the Win32 API ---------------------------------------------------------
// HRSRC is the Entry*, and HGLOBAL is the same pointer: there is no separate "loaded"
// state to model when the backing store is a file that is read on demand.

HRSRC FindResource(HINSTANCE, LPCTSTR name, LPCTSTR type) {
    if (!ResourceLoadManifest()) return (HRSRC)0;
    // MAKEINTRESOURCE packs a numeric id into the low word of a pointer. The engine only
    // ever asks by number (dib.cpp uses MAKEINTRESOURCE(wResid)), so a genuine string name
    // is not a case that arises - and would be a silent wrong lookup if treated as one.
    UINT_PTR raw = (UINT_PTR)name;
    if (raw > 0xFFFF) {
        fprintf(stderr, "resources: FindResource by NAME (\"%s\") is not supported; the "
                        "manifest is keyed by numeric id\n", (const char*)name);
        return (HRSRC)0;
    }
    std::map<std::string, Entry>::iterator it = g_entries.find(KeyOf((UINT)raw, type));
    return (it == g_entries.end()) ? (HRSRC)0 : (HRSRC)&it->second;
}

HGLOBAL LoadResource(HINSTANCE, HRSRC h) {
    Entry* e = (Entry*)h;
    if (!e) return (HGLOBAL)0;
    return EnsureBytes(*e) ? (HGLOBAL)e : (HGLOBAL)0;
}

void* LockResource(HGLOBAL h) {
    Entry* e = (Entry*)h;
    if (!e || e->bytes.empty()) return (void*)0;
    return (void*)&e->bytes[0];
}

DWORD SizeofResource(HINSTANCE, HRSRC h) {
    Entry* e = (Entry*)h;
    if (!e) return 0;
    return EnsureBytes(*e) ? (DWORD)e->bytes.size() : 0;
}
