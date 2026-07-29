// gdishim.h - CDC and friends: declarations now, Core Graphics later.
//
// STATUS: every method here is a STUB. They exist so that engine translation
// units which *contain* drawing code will compile and link even when the program
// never draws - dib.cpp defines CDIB::Draw, balloon.cpp defines its outline
// renderer, and the linker demands those symbols whether or not they run.
//
// This is deliberate staging, not a shortcut. The `--avb` milestone decodes every
// ComicArt asset and CRCs the pixels without drawing a single one, so it can be
// validated against oracle/avb/*.golden.json with these stubs in place. Only when
// the app needs to put pixels on screen does CDC need a real Core Graphics
// backend, and at that point the corpus goldens (which DO depend on text metrics
// and path geometry) become the gate.
//
// The stubs abort rather than silently no-op where a caller would be misled by a
// wrong answer: a measurement returning zero would corrupt layout quietly, which
// is exactly the failure mode the oracle exists to prevent. Anything that only
// paints is free to no-op.

#ifndef NATIVE_SHIM_GDISHIM_H
#define NATIVE_SHIM_GDISHIM_H

#include "win32types.h"
#include "mfcshim.h"
#include <stdio.h>
#include <vector>
#include <time.h>
#include <unistd.h>

// Reaching an unimplemented drawing stub is a programming error, not a runtime
// condition - it means a code path started painting before the Core Graphics
// backend existed. Fail loudly and immediately rather than produce a plausible
// wrong number.
#define NATIVE_GDI_UNIMPLEMENTED(what) \
    do { \
        fprintf(stderr, "native: GDI not implemented yet: %s (%s:%d)\n", \
                (what), __FILE__, __LINE__); \
        abort(); \
    } while (0)

// Raster ops and mapping modes used by the engine.
#define SRCCOPY         0x00CC0020
#define SRCAND          0x008800C6
#define SRCPAINT        0x00EE0086
#define SRCINVERT       0x00660046
#define NOTSRCCOPY      0x00330008
#define BLACKNESS       0x00000042
#define WHITENESS       0x00FF0062
#define DSTINVERT       0x00550009
#define MERGECOPY       0x00C000CA
// MERGEPAINT (~src | dst) is the mask half of GDI's transparent-blit pair; bodycam.cpp uses
// it for every avatar mask and aura. See native/shim/cgblit.cpp for how the pair is
// translated to an alpha composite.
#define MERGEPAINT      0x00BB0226
// Binary raster ops (SetROP2). bodycam.cpp XORs its cursor crosshair so the second draw
// erases the first - a no-op backend leaves the crosshair painted, which is a cosmetic
// artefact of the body-cam window and not on any drawing path the page render uses.
#define R2_BLACK        1
#define R2_NOT          6
#define R2_XORPEN       7
#define R2_COPYPEN      13
#define R2_WHITE        16
// PALETTERGB asks GDI to match the nearest palette entry. macOS is true colour, so it is
// the identity - which is also what RGB() means here.
#define PALETTERGB(r, g, b)  RGB(r, g, b)
#define PATCOPY         0x00F00021

#define NULLREGION      1
#define SIMPLEREGION    2
#define COMPLEXREGION   3
#define RGN_AND         1
#define RGN_OR          2
#define RGN_XOR         3
#define RGN_DIFF        4
#define RGN_COPY        5

#define DT_LEFT             0x00000000
#define DT_CENTER           0x00000001
#define DT_RIGHT            0x00000002
#define DT_TOP              0x00000000
#define DT_VCENTER          0x00000004
#define DT_BOTTOM           0x00000008
#define DT_WORDBREAK        0x00000010
#define DT_SINGLELINE       0x00000020
#define DT_NOCLIP           0x00000100
#define DT_CALCRECT         0x00000400
#define DT_NOPREFIX         0x00000800
#define DT_END_ELLIPSIS     0x00008000
#define DT_PATH_ELLIPSIS    0x00004000

// Rect helpers. Real implementations - balloon.cpp uses SetRect to build the
// text-layout rectangle, so a no-op would collapse every balloon's text box.
inline BOOL SetRect(RECT* r, int l, int t, int rt, int b) {
    if (!r) return FALSE;
    r->left = l; r->top = t; r->right = rt; r->bottom = b;
    return TRUE;
}
inline BOOL SetRectEmpty(RECT* r) { return SetRect(r, 0, 0, 0, 0); }
// DrawTextEx: painting, so a no-op - EXCEPT under DT_CALCRECT, where it is a
// MEASUREMENT and callers use the returned rect for layout. Aborting in that case
// rather than returning the untouched rect, for the same reason GetTextExtent
// aborts: a silently wrong height reflows the balloon.
typedef struct tagDRAWTEXTPARAMS { UINT cbSize; int iTabLength, iLeftMargin, iRightMargin; UINT uiLengthDrawn; } DRAWTEXTPARAMS;
inline int DrawTextEx(HDC, LPSTR, int, RECT*, UINT flags, DRAWTEXTPARAMS*) {
    if (flags & DT_CALCRECT) {
        NATIVE_GDI_UNIMPLEMENTED("DrawTextEx with DT_CALCRECT - measurement, needs the frozen glyph table");
    }
    return 0;
}
inline int DrawText(HDC, LPCSTR, int, RECT*, UINT flags) {
    if (flags & DT_CALCRECT) {
        NATIVE_GDI_UNIMPLEMENTED("DrawText with DT_CALCRECT - measurement, needs the frozen glyph table");
    }
    return 0;
}
// Brush origin: panel.cpp saves and restores it around a StretchBlt so the halftone
// pattern does not shift. Painting-side, so no-ops are safe until the Core Graphics
// backend lands; at that point the equivalent is the pattern phase on CGContext.
inline BOOL GetBrushOrgEx(HDC, POINT* p) { if (p) { p->x = 0; p->y = 0; } return TRUE; }
inline BOOL SetBrushOrgEx(HDC, int, int, POINT* prev) { if (prev) { prev->x = 0; prev->y = 0; } return TRUE; }

inline HWND GetFocus() { return (HWND)0; }
inline HWND GetActiveWindow() { return (HWND)0; }
inline HWND GetDesktopWindow() { return (HWND)0; }
// Locale-formatted date/time and the local clock. Real enough to be useful:
// pageview.cpp stamps printed pages with them. Formats are ISO-ish rather than
// locale-driven, which differs from Windows - but nothing golden-tested depends on
// the stamp text, and NSDateFormatter is the right answer if it ever matters.
inline void GetLocalTime(SYSTEMTIME* st) {
    if (!st) return;
    time_t now = time(0);
    struct tm t;
    localtime_r(&now, &t);
    st->wYear = (WORD)(t.tm_year + 1900); st->wMonth = (WORD)(t.tm_mon + 1);
    st->wDayOfWeek = (WORD)t.tm_wday;     st->wDay = (WORD)t.tm_mday;
    st->wHour = (WORD)t.tm_hour;          st->wMinute = (WORD)t.tm_min;
    st->wSecond = (WORD)t.tm_sec;         st->wMilliseconds = 0;
}
inline int GetDateFormat(DWORD, DWORD, const SYSTEMTIME* st, LPCSTR, LPSTR buf, int n) {
    if (!buf || n <= 0) return 0;
    if (!st) { buf[0] = 0; return 0; }
    return snprintf(buf, (size_t)n, "%04u-%02u-%02u", st->wYear, st->wMonth, st->wDay) + 1;
}
inline int GetTimeFormat(DWORD, DWORD, const SYSTEMTIME* st, LPCSTR, LPSTR buf, int n) {
    if (!buf || n <= 0) return 0;
    if (!st) { buf[0] = 0; return 0; }
    return snprintf(buf, (size_t)n, "%02u:%02u:%02u", st->wHour, st->wMinute, st->wSecond) + 1;
}

inline SHORT GetKeyState(int) { return 0; }
inline SHORT GetAsyncKeyState(int) { return 0; }
inline int GetWindowText(HWND, LPSTR buf, int n) { if (buf && n) buf[0] = 0; return 0; }
inline BOOL OffsetRect(RECT* r, int dx, int dy) {
    if (!r) return FALSE;
    r->left += dx; r->right += dx; r->top += dy; r->bottom += dy;
    return TRUE;
}
inline BOOL InflateRect(RECT* r, int dx, int dy) {
    if (!r) return FALSE;
    r->left -= dx; r->right += dx; r->top -= dy; r->bottom += dy;
    return TRUE;
}
inline BOOL CopyRect(RECT* d, const RECT* s) { if (!d || !s) return FALSE; *d = *s; return TRUE; }
inline BOOL IsRectEmpty(const RECT* r) { return !r || r->right <= r->left || r->bottom <= r->top; }
inline BOOL PtInRect(const RECT* r, POINT p) {
    return r && p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom;
}
inline BOOL IntersectRect(RECT* d, const RECT* a, const RECT* b) {
    if (!d || !a || !b) return FALSE;
    d->left = a->left > b->left ? a->left : b->left;
    d->top = a->top > b->top ? a->top : b->top;
    d->right = a->right < b->right ? a->right : b->right;
    d->bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (IsRectEmpty(d)) { SetRectEmpty(d); return FALSE; }
    return TRUE;
}
inline BOOL UnionRect(RECT* d, const RECT* a, const RECT* b) {
    if (!d || !a || !b) return FALSE;
    d->left = a->left < b->left ? a->left : b->left;
    d->top = a->top < b->top ? a->top : b->top;
    d->right = a->right > b->right ? a->right : b->right;
    d->bottom = a->bottom > b->bottom ? a->bottom : b->bottom;
    return TRUE;
}

#define COLOR_WINDOW        5
#define COLOR_WINDOWTEXT    8
#define COLOR_BTNFACE       15
#define COLOR_HIGHLIGHT     13
#define COLOR_HIGHLIGHTTEXT 14
#define COLOR_GRAYTEXT      17
inline COLORREF GetSysColor(int) { return RGB(255, 255, 255); }

#define MM_TEXT         1
#define MM_TWIPS        6
#define MM_ANISOTROPIC  8
#define TA_LEFT         0
#define TA_RIGHT        2
#define TA_CENTER       6
#define TA_TOP          0
#define TA_BOTTOM       8
#define TA_BASELINE     24
#define HORZRES         8
#define VERTRES         10
#define LOGPIXELSX_     88

#define TRANSPARENT     1
#define OPAQUE          2

#define ALTERNATE       1
#define WINDING         2

#define DIB_RGB_COLORS  0
#define DIB_PAL_COLORS  1
#define CBM_INIT        0x04
inline HBITMAP CreateDIBitmap(HDC, const BITMAPINFOHEADER*, DWORD, const void*, const BITMAPINFO*, UINT) { return (HBITMAP)0; }

#define PS_SOLID        0
#define PS_DASH         1
#define PS_DOT          2
#define PS_NULL         5

#define ANSI_CHARSET            0
#define DEFAULT_CHARSET         1
#define SHIFTJIS_CHARSET        128
#define SYMBOL_CHARSET          2
#define MAC_CHARSET             77
#define HANGEUL_CHARSET         129
#define JOHAB_CHARSET           130
#define GB2312_CHARSET          134
#define CHINESEBIG5_CHARSET     136
#define GREEK_CHARSET           161
#define TURKISH_CHARSET         162
#define VIETNAMESE_CHARSET      163
#define HEBREW_CHARSET          177
#define ARABIC_CHARSET          178
#define BALTIC_CHARSET          186
#define RUSSIAN_CHARSET         204
#define THAI_CHARSET            222
#define EASTEUROPE_CHARSET      238
#define OEM_CHARSET             255
#define OUT_DEFAULT_PRECIS      0
#define CLIP_DEFAULT_PRECIS     0
#define DEFAULT_QUALITY         0
#define DEFAULT_PITCH           0
#define FF_DONTCARE             0
#define DEFAULT_GUI_FONT        17
#define SYSTEM_FONT             13
#define ANSI_VAR_FONT           12
#define NULL_BRUSH              5
#define WHITE_BRUSH             0
#define BLACK_PEN               7
#define NULL_PEN                8
inline void* GetStockObject(int) { return (void*)0; }
// GetObject copies a GDI object's descriptor out. Zero-fills and reports failure:
// there is no real HFONT behind any handle here, and the callers (rtfctrl.cpp,
// chat.cpp's GUI-font setup) check the result.
inline int GetObject(void*, int n, void* buf) { if (buf && n > 0) memset(buf, 0, (size_t)n); return 0; }

#define FW_NORMAL               400
#define FW_REGULAR              400
#define FW_LIGHT                300
#define FW_SEMIBOLD             600
#define FW_BOLD                 700
#define LF_FACESIZE             32

typedef struct tagLOGFONT {
    LONG  lfHeight;
    LONG  lfWidth;
    LONG  lfEscapement;
    LONG  lfOrientation;
    LONG  lfWeight;
    BYTE  lfItalic;
    BYTE  lfUnderline;
    BYTE  lfStrikeOut;
    BYTE  lfCharSet;
    BYTE  lfOutPrecision;
    BYTE  lfClipPrecision;
    BYTE  lfQuality;
    BYTE  lfPitchAndFamily;
    char  lfFaceName[LF_FACESIZE];
} LOGFONT, *LPLOGFONT;

typedef struct tagTEXTMETRIC {
    LONG tmHeight;
    LONG tmAscent;
    LONG tmDescent;
    LONG tmInternalLeading;
    LONG tmExternalLeading;
    LONG tmAveCharWidth;
    LONG tmMaxCharWidth;
    LONG tmWeight;
    LONG tmOverhang;
    LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY;
    char tmFirstChar;
    char tmLastChar;
    char tmDefaultChar;
    char tmBreakChar;
    BYTE tmItalic;
    BYTE tmUnderlined;
    BYTE tmStruckOut;
    BYTE tmPitchAndFamily;
    BYTE tmCharSet;
} TEXTMETRIC, *LPTEXTMETRIC;

typedef struct tagPALETTEENTRY {
    BYTE peRed, peGreen, peBlue, peFlags;
} PALETTEENTRY;

// PALETTEENTRY must precede this: LOGPALETTE ends with a palPalEntry[1] flexible array,
// which bodycam.cpp writes through after over-allocating the struct. Declaring the array
// (rather than leaving it off) is what makes that indexing legal.
typedef struct tagLOGPALETTE {
    WORD palVersion;
    WORD palNumEntries;
    PALETTEENTRY palPalEntry[1];
} LOGPALETTE;

// AfxGetResourceHandle: the module whose resources to search. There is only one module
// here, and the resource layer ignores the handle entirely (see resources.cpp), so the
// value only has to be non-NULL - dib.cpp passes it straight to FindResource.
inline HINSTANCE AfxGetResourceHandle() { return (HINSTANCE)1; }

#define MAKEINTRESOURCE(i)  ((LPCTSTR)(uintptr_t)(i))
// The global ::DeleteObject, distinct from CGdiObject::DeleteObject. bodycam.h
// calls it on a raw HBITMAP. Painting-side, so a no-op success is safe; it does
// mean the native build leaks any GDI handle that path allocates, which is moot
// while nothing allocates one.
inline BOOL DeleteObject(void*) { return TRUE; }

// Font enumeration. format.cpp enumerates installed faces to validate the comic
// font. Reporting zero fonts is honest for a stub - but note the port must NOT
// derive text metrics from whatever it finds: RULEBOOK 5 requires the frozen glyph
// table, so enumeration only ever answers "does this face exist".
typedef struct tagNEWTEXTMETRIC {
    TEXTMETRIC tm;
    DWORD ntmFlags;
    UINT  ntmSizeEM, ntmCellHeight, ntmAvgWidth;
} NEWTEXTMETRIC, *LPNEWTEXTMETRIC;
typedef struct tagFONTSIGNATURE { DWORD fsUsb[4], fsCsb[2]; } FONTSIGNATURE;
typedef struct tagNEWTEXTMETRICEX { NEWTEXTMETRIC ntmTm; FONTSIGNATURE ntmFontSig; } NEWTEXTMETRICEX;
typedef struct tagENUMLOGFONTEX {
    LOGFONT elfLogFont;
    char    elfFullName[64], elfStyle[32], elfScript[32];
} ENUMLOGFONTEX;
typedef int (*FONTENUMPROC)(const LOGFONT*, const TEXTMETRIC*, DWORD, LPARAM);

#define FIXED_PITCH     1
#define VARIABLE_PITCH  2
#define MONO_FONT       8
#define TRUETYPE_FONTTYPE 4
#define DEVICE_FONTTYPE   2
#define RASTER_FONTTYPE   1

inline int EnumFontFamiliesEx(HDC, LOGFONT*, FONTENUMPROC, LPARAM, DWORD) { return 0; }
inline int EnumFontFamilies(HDC, LPCTSTR, FONTENUMPROC, LPARAM) { return 0; }

// Global ::GetDC / ::ReleaseDC take an HWND, unlike CWnd's members.
inline HDC GetDC(HWND) { return (HDC)0; }
inline int ReleaseDC(HWND, HDC) { return 0; }
// The ERROR_* codes and SetLastError/GetLastError moved to win32types.h: winnls.h sits
// ABOVE this header in the include order and needs to set ERROR_INSUFFICIENT_BUFFER.

// Raw HDC font entry points, used with ::-qualified calls in format.cpp alongside
// the CDC members.
inline HFONT   CreateFontIndirect(const LOGFONT*) { return (HFONT)0; }
inline void*   SelectObject(HDC, void*) { return (void*)0; }
inline int     GetTextFace(HDC, int n, LPTSTR buf) { if (buf && n > 0) buf[0] = 0; return 0; }
inline UINT    GetTextCharset(HDC) { return ANSI_CHARSET; }
inline BOOL    GetTextMetrics(HDC, LPTEXTMETRIC tm) { if (tm) memset(tm, 0, sizeof(*tm)); return FALSE; }
inline BOOL    GetTextExtentPoint32(HDC, LPCTSTR, int, LPSIZE) { return FALSE; }

// SetLastError/GetLastError: see win32types.h. They are REAL now - ccommon.cpp branches
// on GetLastError() == ERROR_INSUFFICIENT_BUFFER, and chatsrv.cpp treats
// GetLastError() == WSAEWOULDBLOCK as a successful non-blocking connect.

#define CFM_STRIKEOUT   0x00000008
#define CFE_STRIKEOUT   0x0008
#define CFM_OFFSET      0x10000000
// The Win32 resource API, REAL now: served from the res/ files chat.rc names, via
// native/resources/bitmaps.json. See native/shim/resources.h - this is what makes the
// emotion wheel's face icons load. Declarations only; the lookup needs a JSON parser and
// file I/O, which this header must stay clear of.
HRSRC   FindResource(HINSTANCE, LPCTSTR name, LPCTSTR type);
HGLOBAL LoadResource(HINSTANCE, HRSRC);
void*   LockResource(HGLOBAL);
DWORD   SizeofResource(HINSTANCE, HRSRC);
// Nothing to free: the bytes are cached for the process's lifetime, which is what a locked
// Win32 resource amounts to. dib.cpp says as much - "not required to unlock or free the
// resource in Win32".
inline BOOL    FreeResource(HGLOBAL) { return TRUE; }

// Raw GDI blit entry points used directly (not via CDC) by dib.cpp's Draw path.
// Painting, so no-ops are safe until there is a window; see the class comment on
// CDC for why measurement is treated differently.
inline int SetDIBitsToDevice(HDC, int, int, DWORD, DWORD, int, int, UINT, UINT,
                             const void*, const BITMAPINFO*, UINT) { return 0; }
// Defined out-of-line in native/shim/cgblit.cpp: it needs Core Graphics, and this header
// is included by all ~35 translation units. A DC with no CGContext still draws nothing, so
// the oracle path is unaffected.
int StretchDIBits(HDC hdc, int xDst, int yDst, int wDst, int hDst,
                  int xSrc, int ySrc, int wSrc, int hSrc,
                  const void* bits, const BITMAPINFO* bmi, UINT usage, DWORD rop);
inline int SetStretchBltMode(HDC, int) { return 0; }
// CreateDIBSection allocates a bitmap whose bits the caller writes directly. bodycam.cpp
// uses it for the body-cam window's offscreen buffer. Reports failure: no window exists, and
// handing back a buffer with no way to present it would be a lie the caller cannot detect.
inline void* CreateDIBSection(HDC, const BITMAPINFO*, UINT, void** ppvBits, HANDLE, DWORD) {
    if (ppvBits) *ppvBits = 0;
    return 0;
}
inline int GetDeviceCaps(HDC, int) { return 0; }
#define BLACKONWHITE     1
#define WHITEONBLACK     2
#define COLORONCOLOR 3
#define HALFTONE     4
#define STRETCH_ANDSCANS      1
#define STRETCH_ORSCANS       2
#define STRETCH_DELETESCANS   3
#define STRETCH_HALFTONE      4
#define LOGPIXELSX   88
#define LOGPIXELSY   90
// GetDeviceCaps indices and the raster-capability bit bodycam.cpp tests when deciding
// whether the display is palettised.
#define BITSPIXEL    12
#define PLANES       14
#define NUMCOLORS    24
#define RASTERCAPS   38
#define RC_PALETTE   0x0100

// ::GetDIBits - the global form. bodycam.cpp calls it twice with a NULL bits pointer to
// read back just the BITMAPINFOHEADER (and again for BI_BITFIELDS masks). Returns 0
// scanlines copied, i.e. failure, which leaves the caller's header as it initialised it.
// Honest for a build with no display: guessing a bit depth here would silently change the
// format decisions that follow.
static inline int GetDIBits(HDC, HBITMAP, UINT, UINT, void*, BITMAPINFO*, UINT) { return 0; }

// CHARFORMAT - the rich-edit character-format struct. chat.h carries one for the
// text-view font settings, so the type must exist even though no rich edit
// control does.
#define CFM_BOLD        0x00000001
#define CFM_ITALIC      0x00000002
#define CFM_UNDERLINE   0x00000004
#define CFM_COLOR       0x40000000
#define CFM_SIZE        0x80000000
#define CFM_FACE        0x20000000
#define CFM_CHARSET     0x08000000
#define CFE_BOLD        0x0001
#define CFE_ITALIC      0x0002
#define CFE_UNDERLINE   0x0004
typedef struct _charformat {
    UINT    cbSize;
    DWORD   dwMask;
    DWORD   dwEffects;
    LONG    yHeight;
    LONG    yOffset;
    COLORREF crTextColor;
    BYTE    bCharSet;
    BYTE    bPitchAndFamily;
    char    szFaceName[LF_FACESIZE];
} CHARFORMAT;
inline HINSTANCE AfxGetInstanceHandle() { return (HINSTANCE)0; }


// Owner-draw and measure-item structures. These reach the engine core only as
// parameters on handler declarations in headers (memblst.h, userlist.h); no
// owner-draw code runs natively.
typedef struct tagDRAWITEMSTRUCT {
    UINT     CtlType, CtlID, itemID, itemAction, itemState;
    HWND     hwndItem;
    HDC      hDC;
    RECT     rcItem;
    ULONG_PTR itemData;
} DRAWITEMSTRUCT, *LPDRAWITEMSTRUCT;

typedef struct tagMEASUREITEMSTRUCT {
    UINT     CtlType, CtlID, itemID, itemWidth, itemHeight;
    ULONG_PTR itemData;
} MEASUREITEMSTRUCT, *LPMEASUREITEMSTRUCT;

typedef struct tagCREATESTRUCT {
    LPVOID   lpCreateParams;
    HINSTANCE hInstance;
    HMENU    hMenu;
    HWND     hwndParent;
    int      cy, cx, y, x;
    LONG     style;
    LPCSTR   lpszName, lpszClass;
    DWORD    dwExStyle;
} CREATESTRUCT, *LPCREATESTRUCT;

typedef struct tagWINDOWPOS {
    HWND hwnd, hwndInsertAfter;
    int  x, y, cx, cy;
    UINT flags;
} WINDOWPOS, *LPWINDOWPOS;

typedef struct tagMINMAXINFO {
    POINT ptReserved, ptMaxSize, ptMaxPosition, ptMinTrackSize, ptMaxTrackSize;
} MINMAXINFO, *LPMINMAXINFO;

class CGdiObject : public CObject {
public:
    void* m_hObject;
    CGdiObject() : m_hObject(0) {}
    BOOL DeleteObject() { return TRUE; }
};

class CFont : public CGdiObject {
public:
    BOOL CreateFontIndirect(const LOGFONT* lf) { if (lf) m_lf = *lf; return TRUE; }
    BOOL CreatePointFont(int, LPCTSTR, CDC* = 0) { return TRUE; }
    int GetLogFont(LOGFONT* lf) { if (lf) *lf = m_lf; return sizeof(LOGFONT); }
    LOGFONT m_lf;
    CFont() { memset(&m_lf, 0, sizeof(m_lf)); }
};

class CPen : public CGdiObject {
public:
    // Style/width/colour are RECORDED, not discarded: balloon.cpp creates a thick solid pen
    // for the outline and a separate dashed "nimbus" pen for whisper balloons, and picks
    // between them per balloon. A pen that forgets its width strokes every balloon the same.
    int      m_style;
    int      m_width;
    COLORREF m_color;
    CPen() : m_style(0 /*PS_SOLID*/), m_width(0), m_color(0) {}
    CPen(int style, int width, COLORREF c) : m_style(style), m_width(width), m_color(c) {}
    BOOL CreatePen(int style, int width, COLORREF c) {
        m_style = style; m_width = width; m_color = c; return TRUE;
    }
};

class CBrush : public CGdiObject {
public:
    COLORREF m_color;
    BOOL     m_null;          // a NULL_BRUSH fills nothing
    CBrush() : m_color(0), m_null(FALSE) {}
    CBrush(COLORREF c) : m_color(c), m_null(FALSE) {}
    BOOL CreateSolidBrush(COLORREF c) { m_color = c; m_null = FALSE; return TRUE; }
    BOOL CreateStockObject(int i) { m_null = (i == 5 /*NULL_BRUSH*/); return TRUE; }
};

class CBitmap : public CGdiObject {
public:
    BOOL CreateCompatibleBitmap(CDC*, int, int) { return TRUE; }
    BOOL CreateBitmap(int, int, UINT, UINT, const void*) { return TRUE; }
    // panel.cpp calls this through an instance (temp.FromHandle(h)), which is legal
    // for a static member and is how MFC's own samples use it.
    static CBitmap* FromHandle(HBITMAP) { return 0; }
    // bodycam.cpp writes (HBITMAP)bm on a CBitmap value. MFC's CGdiObject provides this
    // through operator HGDIOBJ; spelling it out here keeps the cast working without
    // giving every CGdiObject an implicit conversion to void*.
    operator HBITMAP() const { return (HBITMAP)0; }
};

class CPalette : public CGdiObject {
public:
    BOOL CreatePalette(LOGPALETTE*) { return TRUE; }
    UINT GetPaletteEntries(UINT, UINT, PALETTEENTRY*) const { return 0; }
};

class CRgn : public CGdiObject {
public:
    BOOL CreateRectRgn(int, int, int, int) { return TRUE; }
    BOOL CreatePolygonRgn(const POINT*, int, int) { return TRUE; }
    int CombineRgn(CRgn*, CRgn*, int) { return 0; }
    BOOL PtInRegion(int, int) const { return FALSE; }
    void GetRgnBox(RECT* r) const { if (r) { r->left = r->top = r->right = r->bottom = 0; } }
};

// ---------------------------------------------------------------------------
// CDC. Text measurement is separated from painting on purpose:
//
//   * Painting stubs no-op. Nothing observable depends on them until there is a
//     window, and the --avb milestone never paints.
//   * MEASUREMENT stubs abort. GetTextExtent feeds CFormatInfo line breaking,
//     which the corpus goldens pin exactly; a zero-width answer would silently
//     reflow every balloon. When this is implemented it must read the FROZEN
//     glyph table (oracle/glyphs/glyphs.json), not Core Text - see RULEBOOK 5.
//     Measuring live would reintroduce the exact platform dependence the frozen
//     table was created to remove.
// ---------------------------------------------------------------------------
class CDC {
public:
    void* m_hDC;
    // MFC exposes m_bPrinting on CDC and panel.cpp branches on it. FALSE here: the
    // native app has no print path (print.cpp is dropped), and reporting TRUE would
    // send panel layout down the printer branch.
    BOOL m_bPrinting;
    // m_pinnedFont starts TRUE: the oracle dumps set up the pinned font before any
    // measurement, and a DC that is never given a font at all is only used for
    // painting. A wrong font is caught at SelectObject time instead.
    CDC() : m_nDcMapMode(MM_TEXT), m_hDC(0), m_bPrinting(FALSE), m_pinnedFont(TRUE),
            m_pCurFont(0), m_nRop2(13 /*R2_COPYPEN*/),
            m_winOrgX(0), m_winOrgY(0), m_pPen(0), m_pBrush(0),
            m_cgPath(0), m_curX(0), m_curY(0), m_hasCur(0),
            m_bkMode(2 /*TRANSPARENT*/), m_textColor(0), m_cgCtx(0),
            m_pendMaskBits(0), m_pendMaskInfo(0),
            m_pendMaskX(0), m_pendMaskY(0), m_pendMaskW(0), m_pendMaskH(0) {
        // GetSafeHdc() has to hand back something the global GDI entry points can turn
        // back into this object, because CDIB::Draw goes through ::StretchDIBits(HDC,...)
        // rather than a CDC member. The CDC pointer IS the handle here - there is no real
        // GDI object to refer to, and inventing a handle table would buy nothing.
        m_hDC = (void*)this;
    }

    // Where painting goes, or NULL for a DC that only measures. The oracle harness never
    // sets this, which is what keeps the 50 goldens out of reach of any drawing change.
    void* m_cgCtx;                  // CGContextRef, as void* to keep this header framework-free

    // GDI has no alpha: a transparent sprite is blitted as MERGEPAINT(mask) then
    // SRCAND(drawing) - see NativeStretchDIBits. The mask arrives first, so it is held
    // here until the drawing that consumes it turns up.
    const void*       m_pendMaskBits;
    const BITMAPINFO* m_pendMaskInfo;
    int m_pendMaskX, m_pendMaskY, m_pendMaskW, m_pendMaskH;
    // The font currently selected, so SelectObject can return the PREVIOUS one - which
    // is the whole mechanism callers use to restore it. Returning the newly selected font
    // instead (as this did) makes every save/restore pair a no-op, and that is why
    // CLabel::WidestWord measured in the balloon font where Windows used the stock one.
    CFont* m_pCurFont;
    HDC GetSafeHdc() const { return (HDC)m_hDC; }
    CFont* GetCurrentFont() const { return 0; }
    // Defined out-of-line in glyphtable_cdc.cpp against the frozen glyph table. NOT a
    // stub: fonts.cpp:83 does
    //     doVKern = (strcmp(szPhysFaceName, "Comic Sans MS") == 0) ? 1 : 0;
    // and then scales the balloon font's leading and baseAdd by it. Returning an empty
    // face name made doVKern 0, so m_leading was 0 instead of -53 and m_lineHeight 345
    // instead of 292 - which changed balloon width, line count and every balloon spline
    // in the corpus.
    // SetROP2 returns the previous mode. Recorded but not honoured: the only user is the
    // body-cam crosshair (see R2_XORPEN above).
    int SetROP2(int mode) { int old = m_nRop2; m_nRop2 = mode; return old; }
    int GetROP2() const { return m_nRop2; }
    int m_nRop2;

    int GetTextFace(int n, LPTSTR buf) const;
    int GetTextFace(CString& s) const;
    CPalette* GetCurrentPalette() const { return 0; }
    virtual ~CDC() {}

    // -- mapping / state (safe no-ops) --
    // The DC's own map mode. Named m_nDcMapMode, NOT m_nMapMode: MFC has both a map
    // mode on CDC (private, reached via Set/GetMapMode) and a PUBLIC m_nMapMode member
    // on CScrollView that pageview.cpp reads directly. Conflating them put the member on
    // the wrong class once already.
    int m_nDcMapMode;

    // Records the mode, because LPtoDP/DPtoLP now branch on it rather than being
    // identity. Returning a fixed value here would silently disable those conversions.
    int SetMapMode(int mode) { int prev = m_nDcMapMode; m_nDcMapMode = mode; return prev; }
    int GetMapMode() const { return m_nDcMapMode; }
    // Logical units per DEVICE PIXEL, which is what a GDI pen width of 0 means. 1440/96 in
    // MM_TWIPS page space; 1 in an MM_TEXT window, where logical units already are pixels.
    // Used by the stroke setup in cgdraw.cpp - see ApplyStroke.
    int OnePixel() const { return m_nDcMapMode == MM_TWIPS ? 15 : 1; }
    int SetBkMode(int m) { int o = m_bkMode; m_bkMode = m; return o; }
    COLORREF SetBkColor(COLORREF c) { return c; }
    COLORREF SetTextColor(COLORREF c) { COLORREF o = m_textColor; m_textColor = c; return o; }
    int SetPolyFillMode(int) { return ALTERNATE; }
    UINT SetTextAlign(UINT) { return 0; }
    UINT GetTextAlign() const { return 0; }
    // These return the PREVIOUS origin in MFC, and pageview.cpp saves and restores
    // through the return value - a BOOL would compile at the call and then restore
    // the wrong origin. Zero here because the shim has no viewport state; that is
    // consistent, since nothing native has moved it.
    CPoint SetViewportOrg(int, int) { return CPoint(0, 0); }
    CPoint SetViewportOrg(POINT) { return CPoint(0, 0); }
    // The window origin is REAL. GDI maps device = logical - windowOrg, and balloon.cpp
    // relies on it: CBWoodringNormal::Draw does OffsetWindowOrg(-m_bbox.Left, -m_bbox.Top) so
    // its traj and text can be expressed in balloon-local coordinates. Ignoring it drew every
    // balloon at the panel origin.
    CPoint SetWindowOrg(int x, int y) {
        CPoint o(m_winOrgX, m_winOrgY); m_winOrgX = x; m_winOrgY = y; return o;
    }
    CPoint SetWindowOrg(POINT p) { return SetWindowOrg((int)p.x, (int)p.y); }
    BOOL OffsetWindowOrg(int dx, int dy) { m_winOrgX += dx; m_winOrgY += dy; return TRUE; }
    int m_winOrgX, m_winOrgY;
    BOOL OffsetViewportOrg(int, int) { return TRUE; }
    BOOL SetWindowExt(int, int) { return TRUE; }
    BOOL SetViewportExt(int, int) { return TRUE; }
    BOOL IsPrinting() const { return FALSE; }
    int SaveDC() { return 1; }
    BOOL RestoreDC(int) { return TRUE; }

    CGdiObject* SelectObject(CGdiObject* p) { return p; }
    CFont* SelectObject(CFont* p);   // records m_pinnedFont; see glyphtable_cdc.cpp
    CPen*   SelectObject(CPen* p)   { CPen* o = m_pPen; if (p) m_pPen = p; return o; }
    CBrush* SelectObject(CBrush* p) { CBrush* o = m_pBrush; if (p) m_pBrush = p; return o; }
    CPen*   m_pPen;
    CBrush* m_pBrush;
    CBitmap* SelectObject(CBitmap* p) { return p; }
    CPalette* SelectPalette(CPalette* p, BOOL) { return p; }
    UINT RealizePalette() { return 0; }
    CGdiObject* SelectStockObject(int) { return 0; }

    // -- coordinate conversion: REAL, not identity --
    //
    // These are not painting helpers; the engine COMPUTES with them. PointsToTwips
    // (pageview.cpp:174) derives the balloon font height by round-tripping a point
    // through LPtoDP, so the no-op that stood here returned 15 instead of -240. The
    // font then failed to match the frozen glyph table and measurement refused - which
    // is how the bug surfaced, and a good argument for making measurement abort.
    //
    // MM_TWIPS: a logical unit is 1/1440 inch with y increasing UPWARD, while device
    // units are pixels with y increasing downward - hence the sign flip on y. At the
    // pinned 96 dpi that is exactly 15 twips per pixel.
    //
    // Verifiable rather than merely plausible: with this mapping PointsToTwips(12)
    // returns -240, which is the lfHeight the glyph table records. Any other mapping
    // breaks that identity, so the table is the oracle for this code too.
    void LPtoDP(POINT* p, int n = 1) const {
        if (!p || m_nDcMapMode != MM_TWIPS) return;
        for (int i = 0; i < n; i++) {
            p[i].x = (LONG)((long long)p[i].x * 96 / 1440);
            p[i].y = (LONG)(-((long long)p[i].y * 96 / 1440));
        }
    }
    void DPtoLP(POINT* p, int n = 1) const {
        if (!p || m_nDcMapMode != MM_TWIPS) return;
        for (int i = 0; i < n; i++) {
            p[i].x = (LONG)((long long)p[i].x * 1440 / 96);
            p[i].y = (LONG)(-((long long)p[i].y * 1440 / 96));
        }
    }
    // Extents, not positions: no sign flip, because a height is a magnitude.
    void LPtoDP(SIZE* z) const {
        if (!z || m_nDcMapMode != MM_TWIPS) return;
        z->cx = (LONG)((long long)z->cx * 96 / 1440);
        z->cy = (LONG)((long long)z->cy * 96 / 1440);
    }
    void DPtoLP(SIZE* z) const {
        if (!z || m_nDcMapMode != MM_TWIPS) return;
        z->cx = (LONG)((long long)z->cx * 1440 / 96);
        z->cy = (LONG)((long long)z->cy * 1440 / 96);
    }
    void LPtoDP(RECT* r) const {
        if (!r) return;
        POINT pts[2];
        pts[0].x = r->left;  pts[0].y = r->top;
        pts[1].x = r->right; pts[1].y = r->bottom;
        LPtoDP(pts, 2);
        r->left = pts[0].x; r->top = pts[0].y;
        r->right = pts[1].x; r->bottom = pts[1].y;
    }
    void DPtoLP(RECT* r) const {
        if (!r) return;
        POINT pts[2];
        pts[0].x = r->left;  pts[0].y = r->top;
        pts[1].x = r->right; pts[1].y = r->bottom;
        DPtoLP(pts, 2);
        r->left = pts[0].x; r->top = pts[0].y;
        r->right = pts[1].x; r->bottom = pts[1].y;
    }

    // -- painting (no-ops) --
    // Path construction and painting, implemented in native/shim/cgdraw.cpp. Out-of-line
    // because it needs Core Graphics, and this header reaches every translation unit.
    //
    // These exist so the ENGINE can draw its own balloons. CBWoodringNormal::Draw builds the
    // outline AND ITS TAIL through CTraj::Draw (BeginPath / MoveTo / PolyBezier / CloseFigure /
    // EndPath) and then StrokeAndFillPath, and draws whisper balloons a second time with a
    // dashed pen. Reimplementing that in the renderer got the body but never the tail.
    BOOL MoveTo(int x, int y);
    BOOL LineTo(int x, int y);
    BOOL Polyline(const POINT*, int) { return TRUE; }
    BOOL Polygon(const POINT*, int) { return TRUE; }
    BOOL PolyBezier(const POINT* pts, int n);
    BOOL PolyBezierTo(const POINT* pts, int n);
    BOOL PolylineTo(const POINT*, int) { return TRUE; }
    BOOL ArcTo(int, int, int, int, int, int, int, int) { return TRUE; }
    BOOL LineTo(POINT p) { return LineTo((int)p.x, (int)p.y); }
    BOOL MoveTo(POINT p) { return MoveTo((int)p.x, (int)p.y); }
    BOOL Rectangle(int, int, int, int) { return TRUE; }
    BOOL Ellipse(int l, int t, int r, int b);
    BOOL Ellipse(const RECT* rc);
    BOOL Rectangle(const RECT*) { return TRUE; }
    BOOL RoundRect(const RECT*, POINT) { return TRUE; }
    BOOL Arc(int, int, int, int, int, int, int, int) { return TRUE; }
    BOOL BeginPath();
    BOOL EndPath();
    BOOL StrokeAndFillPath();
    BOOL StrokePath();
    BOOL FillPath();
    BOOL CloseFigure();
    BOOL TextOut(int x, int y, LPCTSTR s, int len);
    BOOL TextOut(int x, int y, const CString& s);

    // Path state. m_cgPath is non-NULL only between BeginPath and EndPath.
    void* m_cgPath;          // CGMutablePathRef
    int   m_curX, m_curY;    // current point, in logical coords
    int   m_hasCur;
    int   m_bkMode;
    COLORREF m_textColor;
    BOOL ExtTextOut(int, int, UINT, const RECT*, LPCTSTR, UINT, const int*) { return TRUE; }
    int DrawText(LPCTSTR, int, RECT*, UINT) { return 0; }
    BOOL BitBlt(int, int, int, int, CDC*, int, int, DWORD) { return TRUE; }
    BOOL StretchBlt(int, int, int, int, CDC*, int, int, int, int, DWORD) { return TRUE; }
    int SetStretchBltMode(int) { return 0; }
    BOOL PatBlt(int, int, int, int, DWORD) { return TRUE; }
    BOOL DrawIcon(int, int, HICON) { return TRUE; }
    BOOL DrawIcon(POINT, HICON) { return TRUE; }
    void FillRect(const RECT*, CBrush*) {}
    // Real: panel.cpp paints the panel background with it, and CLabel::EraseRect clears
    // behind text.
    void FillSolidRect(const RECT* rc, COLORREF c);
    void FillSolidRect(int x, int y, int w, int h, COLORREF c);
    void Draw3dRect(const RECT*, COLORREF, COLORREF) {}
    void FrameRect(const RECT*, CBrush*) {}
    BOOL InvertRect(const RECT*) { return TRUE; }
    BOOL DrawFocusRect(const RECT*) { return TRUE; }
    BOOL CreateCompatibleDC(CDC*) { return TRUE; }
    BOOL DeleteDC() { return TRUE; }
    BOOL SelectClipRgn(CRgn*) { return TRUE; }
    BOOL SelectClipRgn(CRgn*, int) { return TRUE; }
    int IntersectClipRect(int, int, int, int) { return 0; }
    int IntersectClipRect(const RECT*) { return 0; }
    int ExcludeClipRect(const RECT*) { return 0; }
    // GetClipBox is a QUERY, but a painting-side one: panel.cpp saves and restores
    // the clip box around a draw. Returning an empty rect is safe because nothing
    // native paints yet; when the Core Graphics backend lands this must return the
    // real clip, since panel.cpp compares against it to decide what to redraw.
    virtual int GetClipBox(RECT* r) const {
        if (r) { r->left = r->top = r->right = r->bottom = 0; }
        return 0;   // NULLREGION
    }

    // -- MEASUREMENT: from the frozen glyph table, never from the platform --
    //
    // Implemented out-of-line in glyphtable_cdc.cpp so this header does not have to
    // include the table. The contract (RULEBOOK 5): sum pinned per-character
    // advances, and refuse rather than guess for anything the table does not cover.
    //
    // m_pinnedFont tracks whether the font currently selected into this DC is the
    // one the table was captured for. SelectObject(CFont*) updates it. Measuring
    // with any other font would be silently wrong - a plausible number from the
    // wrong font is worse than a crash, because it only shows up as a golden
    // mismatch several layers downstream.
    SIZE GetTextExtent(LPCTSTR s, int len) const;
    SIZE GetTextExtent(const CString& s) const;
    SIZE GetOutputTextExtent(LPCTSTR s, int len) const { return GetTextExtent(s, len); }
    BOOL GetTextMetrics(TEXTMETRIC* tm) const;
    BOOL GetCharWidth(UINT first, UINT last, int* buf) const;
    int GetDeviceCaps(int index) const;

    BOOL m_pinnedFont;
};

// Both bind to the window's current paint context, so `CPaintDC dc(this)` at the top of an
// untouched OnPaint draws to the screen. Defined out of line in msgmap.cpp because CWnd is
// declared in mfcui.h, which includes THIS header - the members are not visible yet here.
//
// The map mode is left at the CDC default of MM_TEXT: a window's client coordinates are
// device pixels with y positive downward, which is what NativeWndPaint sets up the CTM for.
class CClientDC : public CDC {
public:
    CClientDC(CWnd* pWnd);
};
class CPaintDC : public CDC {
public:
    CPaintDC(CWnd* pWnd);
};

// ---------------------------------------------------------------------------
// CFile - dib.h declares Save(CFile*) so the type must exist. Only the members
// the engine touches are here.
// ---------------------------------------------------------------------------
class CFile : public CObject {
public:
    enum OpenFlags {
        modeRead = 0x0000, modeWrite = 0x0001, modeReadWrite = 0x0002,
        modeCreate = 0x1000, modeNoTruncate = 0x2000,
        shareDenyNone = 0x0040, shareDenyWrite = 0x0020, shareExclusive = 0x0010,
        typeBinary = 0x4000, typeText = 0x8000
    };
    enum SeekPosition { begin = 0, current = 1, end = 2 };

    CFile() : m_fp(0) {}
    virtual ~CFile() { Close(); }

    virtual BOOL Open(LPCTSTR name, UINT flags, void* = 0) {
        const char* mode = (flags & modeCreate) ? "wb"
                         : (flags & (modeWrite | modeReadWrite)) ? "r+b" : "rb";
        // Backslash-separated paths: see NativePath in win32types.h. The engine formats
        // asset paths Win32-style, and this is where an avatar or backdrop is opened.
        char pbuf[NATIVE_PATH_MAX];
        m_fp = name ? fopen(NativePath(name, pbuf, sizeof(pbuf)), mode) : 0;
        return m_fp != 0;
    }
    virtual UINT Read(void* buf, UINT n)  { return m_fp ? (UINT)fread(buf, 1, n, m_fp) : 0; }
    virtual void Write(const void* buf, UINT n) { if (m_fp) fwrite(buf, 1, n, m_fp); }
    virtual LONG Seek(LONG off, UINT from) {
        if (!m_fp) return -1;
        fseek(m_fp, off, from == begin ? SEEK_SET : from == end ? SEEK_END : SEEK_CUR);
        return (LONG)ftell(m_fp);
    }
    virtual DWORD GetPosition() const { return m_fp ? (DWORD)ftell(m_fp) : 0; }
    virtual DWORD GetLength() const {
        if (!m_fp) return 0;
        long cur = ftell(m_fp); fseek(m_fp, 0, SEEK_END);
        long len = ftell(m_fp); fseek(m_fp, cur, SEEK_SET);
        return (DWORD)len;
    }
    virtual void Close() { if (m_fp) { fclose(m_fp); m_fp = 0; } }
    static void Remove(LPCTSTR path) { if (path) unlink(path); }
    static void Rename(LPCTSTR from, LPCTSTR to) { if (from && to) rename(from, to); }
protected:
    FILE* m_fp;
};

class CMemFile : public CFile {
public:
    CMemFile(UINT = 1024) : m_pos(0) {}
    virtual ~CMemFile() {}
    virtual UINT Read(void* buf, UINT n) {
        if (m_pos >= m_buf.size()) return 0;
        UINT avail = (UINT)(m_buf.size() - m_pos);
        if (n > avail) n = avail;
        memcpy(buf, &m_buf[m_pos], n);
        m_pos += n;
        return n;
    }
    virtual void Write(const void* buf, UINT n) {
        if (m_pos + n > m_buf.size()) m_buf.resize(m_pos + n);
        memcpy(&m_buf[m_pos], buf, n);
        m_pos += n;
    }
    virtual LONG Seek(LONG off, UINT from) {
        size_t base = (from == begin) ? 0 : (from == end) ? m_buf.size() : m_pos;
        long p = (long)base + off;
        m_pos = (size_t)(p < 0 ? 0 : p);
        return (LONG)m_pos;
    }
    void SeekToBegin() { m_pos = 0; }
    LONG SeekToEnd() { m_pos = m_buf.size(); return (LONG)m_pos; }
    virtual DWORD GetPosition() const { return (DWORD)m_pos; }
    virtual DWORD GetLength() const { return (DWORD)m_buf.size(); }
    virtual void Close() { m_pos = 0; }
    // Exposed so a caller can read out what was serialised.
    const unsigned char* Buffer() const { return m_buf.empty() ? 0 : &m_buf[0]; }
private:
    std::vector<unsigned char> m_buf;
    size_t m_pos;
};

// ---------------------------------------------------------------------------
// CArchive's string methods, defined here rather than inline in the class because
// they touch CFile and CString, neither of which is complete at that point in
// mfcshim.h. See the CArchive comment there on why no MFC framing is emulated.
// ---------------------------------------------------------------------------
inline void CArchive::WriteString(LPCTSTR s) {
    if (m_pFile && s) m_pFile->Write(s, (UINT)strlen(s));
}
inline BOOL CArchive::ReadString(CString& s) {
    s.Empty();
    return FALSE;
}

// ---------------------------------------------------------------------------
// Late additions. Appended rather than slotted into the sections above because
// insertion-point mistakes in these headers have twice produced a silent no-op
// (a replace whose anchor did not match) and once a semantic error (m_nDcMapMode put
// on CDC instead of CScrollView). Appending is verifiable at a glance.
// ---------------------------------------------------------------------------

typedef struct tagSCROLLINFO {
    UINT cbSize, fMask;
    int  nMin, nMax;
    UINT nPage;
    int  nPos, nTrackPos;
} SCROLLINFO, *LPSCROLLINFO;
#define SIF_RANGE       0x0001
#define SIF_PAGE        0x0002
#define SIF_POS         0x0004
#define SIF_DISABLENOSCROLL 0x0008
#define SIF_TRACKPOS    0x0010
#define SIF_ALL         (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)

#define SIZE_RESTORED   0
#define SIZE_MINIMIZED  1
#define SIZE_MAXIMIZED  2

#define TTF_IDISHWND    0x0001
#define TTF_CENTERTIP   0x0002
#define TTF_RTLREADING  0x0004
#define TTF_SUBCLASS    0x0010
#define TTF_NOTBUTTON   0x0080
#define TTF_ALWAYSTIP   0x0200

// Debug output. Goes to stderr under an opt-in define rather than unconditionally:
// the engine calls it on paths that run per message, and the dumps' stderr is read
// by CI.
#ifdef NATIVE_SHIM_TRACE
static inline void OutputDebugString(LPCSTR s) { if (s) fputs(s, stderr); }
#else
static inline void OutputDebugString(LPCSTR) {}
#endif

// MulDiv, which dpiscale.h's DpiScale() is built on. Real implementation: it rounds
// to nearest and the engine scales pixel sizes with it, so truncating would shift
// icon and panel geometry by a pixel here and there.
static inline int MulDiv(int a, int b, int c) {
    if (c == 0) return -1;
    long long n = (long long)a * (long long)b;
    long long half = (c > 0) ? (c / 2) : -(c / 2);
    return (int)((n + (n >= 0 ? half : -half)) / c);
}

// Raw-HDC blit and DC lifecycle entry points, used ::-qualified alongside the CDC
// members (protsupp.cpp builds a scaled member-list icon this way). Painting, so
// inert until there is a backend.
static inline HDC CreateCompatibleDC(HDC) { return (HDC)0; }
static inline BOOL DeleteDC(HDC) { return TRUE; }
static inline HBITMAP CreateCompatibleBitmap(HDC, int, int) { return (HBITMAP)0; }
static inline BOOL StretchBlt(HDC, int, int, int, int, HDC, int, int, int, int, DWORD) { return TRUE; }
static inline BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD) { return TRUE; }
static inline void* SelectObject_HDC(HDC, void*) { return (void*)0; }

#endif // NATIVE_SHIM_GDISHIM_H
