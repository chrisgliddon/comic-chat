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
#define MB_PRECOMPOSED  0x00000001
#define MB_COMPOSITE    0x00000002
#define MB_ERR_INVALID_CHARS 0x00000008
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
// The width/kana mappings. core/ccommon.cpp's bSB2DBKatakana uses LCMAP_FULLWIDTH for
// the Shift-JIS half-width-katakana conversion. LCMapString below does not implement
// them - it is the single-byte CP-1252 path only - which is consistent with
// IsDBCSLeadByteEx reporting no lead bytes, and with Tier-1 #13 (CJK) being deferred
// scope rather than silently guessed at.
#define LCMAP_HALFWIDTH 0x00400000
#define LCMAP_FULLWIDTH 0x00800000
#define LCMAP_HIRAGANA  0x00100000
#define LCMAP_KATAKANA  0x00200000
static inline unsigned char Cp1252Upper(unsigned char c) {
    if (c >= 0x61 && c <= 0x7A) return (unsigned char)(c - 0x20);
    if (c >= 0xE0 && c <= 0xF6) return (unsigned char)(c - 0x20);
    if (c >= 0xF8 && c <= 0xFE) return (unsigned char)(c - 0x20);
    switch (c) {
        case 0x9A: return 0x8A;   // š -> Š
        case 0x9C: return 0x8C;   // œ -> Œ
        case 0x9E: return 0x8E;   // ž -> Ž
        case 0xFF: return 0x9F;   // ÿ -> Ÿ
        default:   return c;      // includes ß (0xDF), µ (0xB5), ÷ (0xF7)
    }
}
static inline unsigned char Cp1252Lower(unsigned char c) {
    if (c >= 0x41 && c <= 0x5A) return (unsigned char)(c + 0x20);
    if (c >= 0xC0 && c <= 0xD6) return (unsigned char)(c + 0x20);
    if (c >= 0xD8 && c <= 0xDE) return (unsigned char)(c + 0x20);
    switch (c) {
        case 0x8A: return 0x9A;
        case 0x8C: return 0x9C;
        case 0x8E: return 0x9E;
        case 0x9F: return 0xFF;
        default:   return c;      // includes × (0xD7)
    }
}

static inline int LCMapString(DWORD, DWORD flags, LPCSTR src, int srcLen, LPSTR dst, int dstLen) {
    if (!src || !dst || dstLen <= 0) return 0;
    int n = (srcLen < 0) ? (int)strlen(src) : srcLen;
    if (n > dstLen) n = dstLen;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)src[i];
        // Same CP-1252 mapping as CharUpperBuff, not toupper() - see the note there.
        dst[i] = (char)((flags & LCMAP_UPPERCASE) ? Cp1252Upper(c)
                      : (flags & LCMAP_LOWERCASE) ? Cp1252Lower(c) : c);
    }
    return n;
}
// The engine calls both the neutral and the explicitly-A spellings.
#define LCMapStringA    LCMapString
#define MAKELCID(lgid, srt) ((DWORD)((((DWORD)((WORD)(srt))) << 16) | ((DWORD)((WORD)(lgid)))))
#define SUBLANG_DEFAULT 0x01
#define MAKELANGID(p, s)  ((WORD)(((WORD)(s) << 10) | (WORD)(p)))
#define LOCALE_USER_DEFAULT 0x0400
#define LOCALE_SYSTEM_DEFAULT 0x0800

typedef struct _cpinfo { UINT MaxCharSize; BYTE DefaultChar[2]; BYTE LeadByte[12]; } CPINFO;

static inline BOOL IsDBCSLeadByteEx(UINT, BYTE) { return FALSE; }
static inline BOOL GetCPInfo(UINT, CPINFO* i) { if (i) { memset(i, 0, sizeof(*i)); i->MaxCharSize = 1; } return TRUE; }
static inline UINT GetACP() { return CP_ACP; }
// CharUpperBuff uppercases in place and returns the count processed. balloon.cpp uses it
// when normalising text for the shout/emphasis heuristics, so it is real - a no-op would
// change which balloons get treated as shouting. Single-byte only, consistent with
// IsDBCSLeadByteEx above.
//
// It maps through Cp1252Upper, NOT toupper()/tolower(). libc's are ASCII-only in the C locale, so
// accented letters passed through unchanged - and that is measurable, not cosmetic:
// corpus 005 shouts "café résumé naïve", and Windows uppercases it to CAFÉ RÉSUMÉ NAÏVE.
// É is 150 twips wide against é's 120, and Ï is 135 against ï's 75, so leaving them
// lowercase made two balloon lines 90 and 60 twips too narrow.
//
// The exceptions are the ones CP-1252 actually has: ÿ->Ÿ crosses into the 0x80-0x9F block,
// š/œ/ž likewise, ß has no single-character uppercase, and ÷ (0xF7) and × (0xD7) sit inside
// the letter ranges without being letters.
static inline DWORD CharUpperBuff(LPSTR s, DWORD n) {
    if (!s) return 0;
    for (DWORD i = 0; i < n; i++) s[i] = (char)Cp1252Upper((unsigned char)s[i]);
    return n;
}
static inline DWORD CharLowerBuff(LPSTR s, DWORD n) {
    if (!s) return 0;
    for (DWORD i = 0; i < n; i++) s[i] = (char)Cp1252Lower((unsigned char)s[i]);
    return n;
}
// --- CP-1252 <-> UTF-16 ----------------------------------------------------------
//
// These are REAL, not stubs. ircproto.cpp's EncodeNick/DecodeNick do
//     if (!(a = MultiByteToWideChar(...))) goto error;
// so a version returning 0 makes every nick encode fail into the ASSERT(0) path and
// fall back to the raw bytes - which is exactly the kind of quiet wrongness that only
// shows up as mangled nicknames on a live server.
//
// CP_ACP is CP-1252 here, matching the build the oracle covers. The 0x80-0x9F range is
// where CP-1252 is NOT Latin-1: those 32 slots hold the smart quotes, dashes, euro and
// so on. Treating the range as identity is the classic mojibake bug, so it gets a real
// table. The five slots Windows leaves undefined (0x81 0x8D 0x8F 0x90 0x9D) map to the
// same scalar value, which is what Windows does.
// lstrlenW: the wide half of lstrlen. ccommon.cpp measures UTF-16 buffers with it.
static inline int lstrlenW(LPCWSTR s) {
    if (!s) return 0;
    int n = 0;
    while (s[n]) n++;
    return n;
}

static const WCHAR kCp1252High[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

// srcLen < 0 means "null-terminated, and count the terminator", per the Win32 contract.
// dstLen == 0 means "just tell me how much room you need".
static inline int MultiByteToWideChar(UINT, DWORD, LPCSTR src, int srcLen,
                                     LPWSTR dst, int dstLen) {
    if (!src) return 0;
    int n = (srcLen < 0) ? (int)strlen(src) + 1 : srcLen;
    if (dstLen == 0) return n;
    // ccommon.cpp's bConvertString distinguishes a short buffer from any other failure
    // by reading GetLastError(), so the 0 return has to be accompanied by the code.
    if (!dst || dstLen < n) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = (c >= 0x80 && c <= 0x9F) ? kCp1252High[c - 0x80] : (WCHAR)c;
    }
    return n;
}

static inline int WideCharToMultiByte(UINT, DWORD, LPCWSTR src, int srcLen,
                                      LPSTR dst, int dstLen,
                                      LPCSTR defaultChar, LPBOOL usedDefault) {
    if (!src) return 0;
    int n = srcLen;
    if (n < 0) { n = 0; while (src[n]) n++; n++; }
    if (dstLen == 0) return n;
    if (!dst || dstLen < n) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    char sub = (defaultChar && *defaultChar) ? *defaultChar : '?';
    BOOL used = FALSE;
    for (int i = 0; i < n; i++) {
        WCHAR w = src[i];
        if (w < 0x80 || (w >= 0xA0 && w <= 0xFF)) {
            dst[i] = (char)w;
            continue;
        }
        // Reverse the high table; anything else is unrepresentable in CP-1252.
        int found = -1;
        for (int k = 0; k < 32; k++) if (kCp1252High[k] == w) { found = k; break; }
        if (found >= 0) {
            dst[i] = (char)(0x80 + found);
        } else {
            dst[i] = sub;
            used = TRUE;
        }
    }
    if (usedDefault) *usedDefault = used;
    return n;
}

// --- character type classification -----------------------------------------------
// GetStringTypeEx's return value gates DecodeNickForScreen, so it must succeed. The
// body that consumes wTypeInfo is inside #if 0 upstream, but the flags are filled in
// properly anyway rather than being left as garbage.
#define CT_CTYPE1   0x00000001
#define CT_CTYPE2   0x00000002
#define CT_CTYPE3   0x00000004

#define C1_UPPER    0x0001
#define C1_LOWER    0x0002
#define C1_DIGIT    0x0004
#define C1_SPACE    0x0008
#define C1_PUNCT    0x0010
#define C1_CNTRL    0x0020
#define C1_BLANK    0x0040
#define C1_XDIGIT   0x0080
#define C1_ALPHA    0x0100
#define C1_DEFINED  0x0200

static inline BOOL GetStringTypeA(DWORD, DWORD infoType, LPCSTR src, int srcLen, WORD* out) {
    if (!src || !out || infoType != CT_CTYPE1) return FALSE;
    int n = (srcLen < 0) ? (int)strlen(src) : srcLen;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)src[i];
        WORD t = 0;
        if (isupper(c))  t |= C1_UPPER;
        if (islower(c))  t |= C1_LOWER;
        if (isdigit(c))  t |= C1_DIGIT;
        if (isspace(c))  t |= C1_SPACE;
        if (ispunct(c))  t |= C1_PUNCT;
        if (iscntrl(c))  t |= C1_CNTRL;
        if (c == ' ' || c == '\t') t |= C1_BLANK;
        if (isxdigit(c)) t |= C1_XDIGIT;
        if (isalpha(c))  t |= C1_ALPHA;
        if (c) t |= C1_DEFINED;
        out[i] = t;
    }
    return TRUE;
}
#define GetStringTypeEx GetStringTypeA

// There is no per-user locale to consult off-Windows, and nothing in the engine
// branches on the value - it is passed straight to GetStringTypeEx above, which
// ignores it. Reporting the US default keeps the CP-1252 story consistent.
#define LANG_US_LCID  0x0409
static inline DWORD GetUserDefaultLCID()   { return LANG_US_LCID; }
static inline DWORD GetSystemDefaultLCID() { return LANG_US_LCID; }
static inline WORD  GetUserDefaultLangID()   { return LANG_US_LCID; }
static inline WORD  GetSystemDefaultLangID() { return LANG_US_LCID; }
static inline int CompareStringA(DWORD, DWORD, LPCSTR a, int, LPCSTR b, int) {
    int r = strcmp(a ? a : "", b ? b : "");
    return r < 0 ? 1 : (r == 0 ? 2 : 3);   // CSTR_LESS_THAN / EQUAL / GREATER_THAN
}
#define CSTR_LESS_THAN      1
#define CSTR_EQUAL          2
#define CSTR_GREATER_THAN   3

#endif
