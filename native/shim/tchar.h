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
#define _tcstol     strtol
#define _tcstod     strtod

// _mbs* are the multibyte-aware variants. For the single-byte code pages this
// build actually runs (CP-1252 and friends), the plain str* functions are
// equivalent; the CJK double-byte path is a documented open question in the plan
// (Tier-1 #13) and is not reached by anything the oracle covers.
#define _mbschr(s, c)   strchr((const char*)(s), (int)(c))
#define _mbsrchr(s, c)  strrchr((const char*)(s), (int)(c))
#define _mbsinc(s)      ((s) + 1)
#define _mbsdec(a, b)   ((b) > (a) ? (b) - 1 : (a))
#define _mbslen         strlen
#define _mbsstr         strstr
#define _mbclen(s)      1
#define _ismbblead(c)   0
#define _ismbstrail(a, b) 0

#endif // NATIVE_SHIM_TCHAR_H
