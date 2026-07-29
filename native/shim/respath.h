// respath.h - locate the bundled glyph table.
//
// The native build reads ONE data file at startup: the frozen glyph table. (chat.rc's string
// table and bitmaps used to be files too; they are compiled in now - see rcdata.h.) Where it
// lives depends on how the binary was invoked, and there are three real cases, not one:
//
//   * from the repo root            oracle/glyphs/glyphs.json
//   * from v2.5-beta-1-modern/      ../oracle/glyphs/glyphs.json
//     This is not hypothetical: corpus inputs.json carries treeDir="." and the Windows
//     oracle runs the harness from that directory, so the native run has to match it to
//     resolve ComicArt the same way.
//   * from inside a .app bundle     <exe>/../Resources/glyphs.json
//     Comic Chat.app/Contents/MacOS/comicchat with the data in Contents/Resources.
//
// An explicit environment variable always wins, so a caller can point at a specific
// capture without arguing with the search order.
//
// Header-only on purpose: the caller is an already-compiled unit, and adding a shared .cpp
// would mean another entry in NATIVE_UNITS for two dozen lines.

#ifndef NATIVE_SHIM_RESPATH_H
#define NATIVE_SHIM_RESPATH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <mach-o/dyld.h>

// Directory containing the running executable, or "" if it cannot be determined.
static inline std::string NativeExeDir() {
    char buf[4096];
    uint32_t n = (uint32_t)sizeof(buf);
    if (_NSGetExecutablePath(buf, &n) != 0) return std::string();
    std::string p(buf);
    size_t slash = p.find_last_of('/');
    return (slash == std::string::npos) ? std::string() : p.substr(0, slash);
}

// Opens the first candidate that exists. On success returns the FILE* and stores the
// path it used in `chosen` (for diagnostics - a message naming the wrong file is far more
// useful than one naming the file we wished for). Returns NULL with `tried` filled in.
static inline FILE* NativeResourceOpen(const char* envVar, const char* relPath,
                                       std::string& chosen, std::string& tried) {
    std::vector<std::string> cands;

    const char* env = envVar ? getenv(envVar) : 0;
    if (env && *env) cands.push_back(env);

    cands.push_back(relPath);
    cands.push_back(std::string("../") + relPath);

    const char* base = strrchr(relPath, '/');
    base = base ? base + 1 : relPath;
    std::string exeDir = NativeExeDir();
    if (!exeDir.empty()) {
        cands.push_back(exeDir + "/../Resources/" + base);   // .app bundle
        cands.push_back(exeDir + "/" + base);                // beside the binary
        cands.push_back(exeDir + "/../../" + relPath);       // native/build/ -> repo root
    }

    tried.clear();
    for (size_t i = 0; i < cands.size(); i++) {
        FILE* f = fopen(cands[i].c_str(), "rb");
        if (f) { chosen = cands[i]; return f; }
        if (i) tried += ", ";
        tried += cands[i];
    }
    return 0;
}

#endif // NATIVE_SHIM_RESPATH_H
