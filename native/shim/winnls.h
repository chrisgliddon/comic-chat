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

#define CP_ACP          0
#define CP_OEMCP        1
#define CP_UTF8         65001
#define CP_SHIFTJIS     932

#define LANG_NEUTRAL    0x00
#define LANG_ENGLISH    0x09
#define LANG_JAPANESE   0x11
#define SUBLANG_DEFAULT 0x01
#define MAKELANGID(p, s)  ((WORD)(((WORD)(s) << 10) | (WORD)(p)))
#define LOCALE_USER_DEFAULT 0x0400
#define LOCALE_SYSTEM_DEFAULT 0x0800

typedef struct _cpinfo { UINT MaxCharSize; BYTE DefaultChar[2]; BYTE LeadByte[12]; } CPINFO;

inline BOOL IsDBCSLeadByteEx(UINT, BYTE) { return FALSE; }
inline BOOL GetCPInfo(UINT, CPINFO* i) { if (i) { memset(i, 0, sizeof(*i)); i->MaxCharSize = 1; } return TRUE; }
inline UINT GetACP() { return CP_ACP; }
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
