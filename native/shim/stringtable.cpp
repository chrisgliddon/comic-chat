// stringtable.cpp - see stringtable.h.

#include "stdafx.h"
#include "stringtable.h"
#include "rcdata.h"

#include <map>

namespace {

// Built once from the linked-in table. A map rather than a linear scan because LoadString is
// called in loops - textpose.cpp walks the whole emotion-rule range at startup.
std::map<UINT, const char*>& Index() {
    static std::map<UINT, const char*> m;
    static bool built = false;
    if (!built) {
        built = true;
        for (int i = 0; i < kNativeRcStringCount; i++)
            m[kNativeRcStrings[i].id] = kNativeRcStrings[i].text;
    }
    return m;
}

}  // namespace

// Kept for the callers that used to prime the table from a file. There is nothing to load any
// more - the strings are compiled in - so this only reports whether any exist, which would be
// a generator failure rather than a runtime condition.
bool StringTableLoad(const char*) { return kNativeRcStringCount > 0; }

const char* StringTableLookup(UINT id) {
    std::map<UINT, const char*>& m = Index();
    std::map<UINT, const char*>::const_iterator it = m.find(id);
    return (it == m.end()) ? 0 : it->second;
}

int StringTableCount() { return kNativeRcStringCount; }

// CString::LoadString, defined here rather than inline in mfcshim.h because that header must
// not depend on the resource data.
BOOL CString::LoadString(UINT id) {
    const char* s = StringTableLookup(id);
    if (!s) { Empty(); return FALSE; }
    *this = s;
    return TRUE;
}
