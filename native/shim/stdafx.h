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
#ifdef __cplusplus
// The National Language Support surface, on the default chain because the real
// windows.h puts it there. balloon.cpp includes <winnls.h> explicitly, but ircproto.cpp
// reaches GetACP/MultiByteToWideChar/GetStringTypeEx through windows.h alone. Inside the
// C++ fence because its converters call SetLastError, which is C++-only.
#include "winnls.h"
// The SSPI credential struct. ircsock.h includes artifacts/inc/CSSPI.H, which covers the
// function table but leaves this to the real sspi.h. Compile-only - see the header.
#include "sspi.h"
#include "mfcshim.h"
#include "gdishim.h"
#include "mfcui.h"
// The engine's stdafx.h includes afxsock.h, so socket types are in scope for every
// translation unit; chatsrv.h and chatsock.h rely on that rather than including
// winsock themselves.
#include "winsock.h"
// Directory enumeration (FindFirstFile / _findfirst). Needed by the shared Tier-2
// dump and by backdrop.cpp's art-directory walk.
#include "io.h"
// dpiscale.h is a PROJECT header that the engine's own stdafx.h includes, so
// protsupp.cpp and bodycam.cpp use DpiScale() without including it themselves. It
// needs MulDiv from gdishim.h, hence its position after that.
#include "dpiscale.h"
#endif // __cplusplus

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

// Include-order artifact, not a missing type. rules.h uses CCNotif* at line 621
// but only notif.h forward-declares it, and in the MFC build the declaration
// arrives via an include chain that our replacement stdafx.h short-circuits.
// Forward-declaring it here restores the same visibility without dragging notif.h
// (and its dialog dependencies) into the platform floor. The class itself is only
// ever used through pointers in the headers the engine core reaches.
class CCNotif;

// The engine's own stdafx.h declares these two globals (guarded by NOGLOBPAL) and
// pageview.cpp takes the address of ghPalette. Declared here for the same reason as
// CCNotif - our replacement stdafx.h short-circuits the include chain that would
// otherwise bring them. Defined in native/nativeglue.cpp.
extern CPalette   ghPalette;
extern LOGPALETTE *gpLogPal;


// ===========================================================================
// min/max LAST, after every standard header has been pulled in.
//
// windows.h defines these as macros and the engine relies on it (bbox.cpp uses bare
// min/max on ints). They are macros rather than std::min/std::max on purpose: the
// engine mixes int/short/double operands freely, which the templates reject.
//
// The ORDER is the load-bearing part. libc++ #undefs min and max to protect
// std::min/std::max, so defining them before <string>/<vector>/<map>/<algorithm>
// - as an earlier version of this shim did, in win32types.h - loses them again on
// any toolchain whose libc++ does that undef.
//
// This was found by CI, not locally: the development machine runs Apple clang 21,
// where the definitions survived, while the macos-14 runner's clang 15 erased them
// and three core files stopped compiling. Exactly the class of bug the native CI
// job was added to catch, on its first run.
// ===========================================================================
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#endif // NATIVE_SHIM_STDAFX_H
