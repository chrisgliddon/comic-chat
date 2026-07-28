// winnls.h - the National Language Support surface. balloon.cpp includes it for the
// code-page constants used when measuring and breaking text.
//
// IsDBCSLeadByteEx and friends report single-byte behaviour, matching the CP-1252
// build the oracle covers. The CJK double-byte path is the deferred Tier-1 #13
// scope question; a shim that guessed at DBCS would corrupt multibyte text
// silently, so it reports "no lead bytes" rather than pretending.
#ifndef NATIVE_SHIM_WINNLS_H
#define NATIVE_SHIM_WINNLS_H
#include "win32types.h"
#include <ctype.h>
#include <string.h>

#define CP_ACP          0
#define CP_OEMCP        1
#define CP_UTF8         65001
#define CP_SHIFTJIS     932

#define LANG_NEUTRAL    0x00
#define LANG_ENGLISH    0x09
#define LANG_JAPANESE   0x11
#define LANG_GREEK      0x08
#define LANG_RUSSIAN    0x19
#define LANG_TURKISH    0x1f
#define LANG_HEBREW     0x0d
#define LANG_ARABIC     0x01
#define LANG_THAI       0x1e
#define LANG_KOREAN     0x12
#define LANG_CHINESE    0x04
#define SUBLANG_NEUTRAL 0x00
#define SORT_DEFAULT    0x0
#define LCMAP_LOWERCASE 0x00000100
#define LCMAP_UPPERCASE 0x00000200
#define LCMAP_SORTKEY   0x00000400
inline int LCMapString(DWORD, DWORD flags, LPCSTR src, int srcLen, LPSTR dst, int dstLen) {
    if (!src || !dst || dstLen <= 0) return 0;
    int n = (srcLen < 0) ? (int)strlen(src) : srcLen;
    if (n > dstLen) n = dstLen;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = (char)((flags & LCMAP_UPPERCASE) ? toupper(c)
                      : (flags & LCMAP_LOWERCASE) ? tolower(c) : c);
    }
    return n;
}
#define MAKELCID(lgid, srt) ((DWORD)((((DWORD)((WORD)(srt))) << 16) | ((DWORD)((WORD)(lgid)))))
#define SUBLANG_DEFAULT 0x01
#define MAKELANGID(p, s)  ((WORD)(((WORD)(s) << 10) | (WORD)(p)))
#define LOCALE_USER_DEFAULT 0x0400
#define LOCALE_SYSTEM_DEFAULT 0x0800

typedef struct _cpinfo { UINT MaxCharSize; BYTE DefaultChar[2]; BYTE LeadByte[12]; } CPINFO;

inline BOOL IsDBCSLeadByteEx(UINT, BYTE) { return FALSE; }
inline BOOL GetCPInfo(UINT, CPINFO* i) { if (i) { memset(i, 0, sizeof(*i)); i->MaxCharSize = 1; } return TRUE; }
inline UINT GetACP() { return CP_ACP; }
// CharUpperBuff uppercases in place and returns the count processed. balloon.cpp
// uses it when normalising text for the shout/emphasis heuristics, so this is a real
// implementation - a no-op would change which balloons get treated as shouting.
// Single-byte only, consistent with IsDBCSLeadByteEx above.
inline DWORD CharUpperBuff(LPSTR s, DWORD n) {
    if (!s) return 0;
    for (DWORD i = 0; i < n; i++) s[i] = (char)toupper((unsigned char)s[i]);
    return n;
}
inline DWORD CharLowerBuff(LPSTR s, DWORD n) {
    if (!s) return 0;
    for (DWORD i = 0; i < n; i++) s[i] = (char)tolower((unsigned char)s[i]);
    return n;
}
inline int MultiByteToWideChar(UINT, DWORD, LPCSTR, int, void*, int) { return 0; }
inline int WideCharToMultiByte(UINT, DWORD, const void*, int, LPSTR, int, LPCSTR, LPBOOL) { return 0; }
inline int CompareStringA(DWORD, DWORD, LPCSTR a, int, LPCSTR b, int) {
    int r = strcmp(a ? a : "", b ? b : "");
    return r < 0 ? 1 : (r == 0 ? 2 : 3);   // CSTR_LESS_THAN / EQUAL / GREATER_THAN
}
#define CSTR_LESS_THAN      1
#define CSTR_EQUAL          2
#define CSTR_GREATER_THAN   3

#endif
