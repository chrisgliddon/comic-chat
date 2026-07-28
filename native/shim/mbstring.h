// mbstring.h - MSVC's multibyte string routines. The _mbs* mappings live in
// tchar.h; this exists because several sources include <mbstring.h> directly.
//
// Single-byte behaviour, matching the CP-1252 build the oracle covers. See the note
// in tchar.h: the CJK double-byte path is the deferred Tier-1 #13 scope question,
// and guessing at DBCS here would corrupt multibyte text silently.
#ifndef NATIVE_SHIM_MBSTRING_H
#define NATIVE_SHIM_MBSTRING_H
#include "tchar.h"
#endif
