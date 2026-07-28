// stringtable.h - the .rc string table, as data.
//
// A Mach-O binary has no PE resource section, but the engine reads BEHAVIOUR from the
// string table rather than only labels: textpose.cpp's InitializeEmotionRules loads the
// emotion-detection rules from it (ID_RULE_SHOUT and friends), so a LoadString that
// returns FALSE leaves the native build with no emotion rules at all and every pose
// decision differs from the goldens.
//
// native/resources/strings.json is generated from chat.rc by native/gen-strings.py.
// An .app must ship it as a resource; it is part of the engine's behaviour, not
// decoration.
//
// The check on all of this is the frozen textpose golden: with the strings loaded, the
// native --textpose dump has to reproduce oracle/textpose/textpose.golden.json, which was
// captured on Windows from the real resource table.

#ifndef NATIVE_SHIM_STRINGTABLE_H
#define NATIVE_SHIM_STRINGTABLE_H

#include "win32types.h"

// Loads and caches. Path defaults to COMIC_CHAT_STRINGS, else
// "native/resources/strings.json". Returns false if missing or malformed.
bool StringTableLoad(const char* path = 0);

// Looks up a resource id. Returns NULL when absent - the caller decides whether that is
// benign (a UI label) or not.
const char* StringTableLookup(UINT id);

int StringTableCount();

#endif // NATIVE_SHIM_STRINGTABLE_H
