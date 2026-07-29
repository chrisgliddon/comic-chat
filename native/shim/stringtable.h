// stringtable.h - the .rc string table, compiled in.
//
// The engine reads BEHAVIOUR from the string table, not only labels: textpose.cpp's
// InitializeEmotionRules loads the emotion-detection rules from it (ID_RULE_SHOUT and
// friends), so a LoadString that returns FALSE leaves the native build with no emotion rules
// at all and every pose decision differs from the goldens.
//
// The strings come from native/shim/rcdata.cpp, generated from chat.rc by
// native/gen-rcdata.py - the same thing rc.exe did, arriving as static data rather than as a
// PE resource section. Nothing is loaded at startup and an .app ships nothing for this.
//
// The check on the extraction is the frozen textpose golden: with these strings compiled in,
// the native --textpose dump has to reproduce oracle/textpose/textpose.golden.json, which was
// captured on Windows from the real resource table.

#ifndef NATIVE_SHIM_STRINGTABLE_H
#define NATIVE_SHIM_STRINGTABLE_H

#include "win32types.h"

// Retained for callers that used to prime the table from a file. Nothing is loaded now; it
// reports whether any strings were compiled in, which would be a generator failure rather
// than a runtime condition. The argument is ignored.
bool StringTableLoad(const char* path = 0);

// Looks up a resource id. Returns NULL when absent - the caller decides whether that is
// benign (a UI label) or not.
const char* StringTableLookup(UINT id);

int StringTableCount();

#endif // NATIVE_SHIM_STRINGTABLE_H
