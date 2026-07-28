// tchar.h - the MBCS half of MSVC's tchar.h.
//
// Only the narrow mappings are provided. The engine is an MBCS build: it indexes
// strings by byte, calls strchr/strlen on them, and carries CP-1252 payloads. A
// _UNICODE variant of this header would be actively wrong, so there isn't one.

#ifndef NATIVE_SHIM_TCHAR_H
#define NATIVE_SHIM_TCHAR_H

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#ifndef _T
#define _T(x)       x
#endif
#ifndef TEXT
#define TEXT(x)     x
#endif
#define _TEXT(x)    x

#define _tcslen     strlen
#define _tcscpy     strcpy
#define _tcsncpy    strncpy
#define _tcscat     strcat
#define _tcscmp     strcmp
#define _tcsicmp    strcasecmp
#define _tcsncmp    strncmp
#define _tcsnicmp   strncasecmp
#define _tcschr     strchr
#define _tcsrchr    strrchr
#define _tcsstr     strstr
#define _tcstok     strtok
#define _tcsdup     strdup
#define _tcslwr(s)  (s)
#define _tcsupr(s)  (s)
#define _stprintf   sprintf
#define _sntprintf  snprintf
#define _tprintf    printf
#define _ftprintf   fprintf
#define _tfopen     fopen
#define _ttoi       atoi
// _itot writes an integer into a buffer in the given radix. Real implementation:
// urlutil.cpp uses it to build a temp-file name, and a stub would produce colliding
// names rather than merely doing nothing.
#include <stdio.h>
static inline char* _itot(int v, char* buf, int radix) {
    if (!buf) return buf;
    if (radix == 16) sprintf(buf, "%x", v);
    else if (radix == 8) sprintf(buf, "%o", v);
    else sprintf(buf, "%d", v);
    return buf;
}
#define _itoa_shim  _itot
#define _tcstol     strtol
#define _tcstod     strtod

// _mbs* are the multibyte-aware variants. For the single-byte code pages this
// build actually runs (CP-1252 and friends), the plain str* functions are
// equivalent; the CJK double-byte path is a documented open question in the plan
// (Tier-1 #13) and is not reached by anything the oracle covers.
// Character classification and navigation, single-byte (see the note above).
#define _istpunct(c)    ispunct((unsigned char)(c))
#define _istspace(c)    isspace((unsigned char)(c))
#define _istalpha(c)    isalpha((unsigned char)(c))
#define _istdigit(c)    isdigit((unsigned char)(c))
#define _istalnum(c)    isalnum((unsigned char)(c))
#define _totupper(c)    toupper((unsigned char)(c))
#define _totlower(c)    tolower((unsigned char)(c))
#define _tcsnccpy       strncpy
#define _tcsnccmp       strncmp
#define _tcsnccnt       strlen

// _mbschr/_mbsrchr take and return `unsigned char*` in MSVC, not `char*`. Macros
// over strchr returned char*, which the engine then assigned to an UCHAR* - and
// clang rejects that conversion between plain char and unsigned char pointers.
// Inline functions with the real signatures instead.
static inline unsigned char* _mbschr(const unsigned char* s, unsigned int c) {
    return (unsigned char*)strchr((const char*)s, (int)c);
}
static inline unsigned char* _mbsrchr(const unsigned char* s, unsigned int c) {
    return (unsigned char*)strrchr((const char*)s, (int)c);
}
#define _mbsinc(s)      ((s) + 1)
#define _mbsdec(a, b)   ((b) > (a) ? (b) - 1 : (a))
#define _mbslen         strlen
#define _mbspbrk(s, set) strpbrk((const char*)(s), (const char*)(set))
#define _mbsstr         strstr
#define _mbclen(s)      1
#define _ismbblead(c)   0
#define _ismbstrail(a, b) 0

#endif // NATIVE_SHIM_TCHAR_H
