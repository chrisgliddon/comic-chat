// stdafx.h - shim precompiled-header stand-in for the native macOS build.
//
// Every engine .cpp starts with #include "stdafx.h". The native build puts
// native/shim on the include path AHEAD of v2.5-beta-1-modern, so this file is
// what they get instead of the MFC one - which is why the engine sources compile
// unmodified.
//
// Consequence worth knowing: adding an include here affects all 92 engine
// translation units at once. Keep it to the platform floor.

#ifndef NATIVE_SHIM_STDAFX_H
#define NATIVE_SHIM_STDAFX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "win32types.h"
#include "mfcshim.h"
#include "gdishim.h"
#include "mfcui.h"

// chat.h and several other engine headers guard with
//   #ifndef __AFXWIN_H__
//     #error include 'stdafx.h' before including this file for PCH
//   #endif
// which is MFC's generated PCH check. Defining it asserts the same contract the
// real header would: "the platform floor is already in scope".
#define __AFXWIN_H__ 1
#define __AFXEXT_H__ 1
#define __AFXOLE_H__ 1

// The engine calls these POSIX-with-underscore spellings throughout (they are the
// MSVC names). Mapped rather than edited at the ~40 call sites.
#define stricmp     strcasecmp
#define strnicmp    strncasecmp
#define _stricmp    strcasecmp
#define _strnicmp   strncasecmp
#ifndef strdup
// strdup is already POSIX; _strdup is the MSVC spelling.
#define _strdup     strdup
#endif

// MSVC's rand()/srand() are NOT libc's. RULEBOOK 4 requires exact replication of
// the MSVC CRT LCG (seed*214013+2531011, >>16, &0x7fff) because panel layout and
// avatar placement consume it and the oracle pins the sequence. Providing them
// here - rather than letting the engine pick up macOS's rand - is what keeps the
// corpus goldens reachable. Defined in native/shim/msvcrand.cpp.
#ifdef __cplusplus
extern "C" {
#endif
int  msvc_rand(void);
void msvc_srand(unsigned int seed);
#ifdef __cplusplus
}
#endif
#define rand()      msvc_rand()
#define srand(s)    msvc_srand(s)
#define RAND_MAX_MSVC 0x7fff


#endif // NATIVE_SHIM_STDAFX_H
