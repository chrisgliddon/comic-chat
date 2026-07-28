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
#define PATCOPY         0x00F00021

#define MM_TEXT         1
#define MM_TWIPS        6
#define MM_ANISOTROPIC  8

#define TRANSPARENT     1
#define OPAQUE          2

#define ALTERNATE       1
#define WINDING         2

#define DIB_RGB_COLORS  0

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
#define FW_NORMAL               400
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

typedef struct tagLOGPALETTE {
    WORD palVersion;
    WORD palNumEntries;
} LOGPALETTE;

typedef struct tagPALETTEENTRY {
    BYTE peRed, peGreen, peBlue, peFlags;
} PALETTEENTRY;

// AfxGetResourceHandle: the engine loads DIBs from Win32 resources. The native
// app has no PE resource section - ComicArt assets come from files - so this
// returns NULL and the resource-loading paths are expected to fail their own
// checks rather than be reached at all.
inline HINSTANCE AfxGetResourceHandle() { return (HINSTANCE)0; }

// Win32 resource API. dib.cpp's CDIB::Load(WORD wResid) loads DIBs from the PE
// resource section, which a Mach-O binary has no equivalent of - ComicArt assets
// come from files instead. These return NULL so that path fails its own checks
// (`if (!hrsrc) return FALSE`) rather than being reached and misbehaving.
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
#define ERROR_SUCCESS               0L
#define ERROR_INVALID_PARAMETER     87L
#define ERROR_FILE_NOT_FOUND        2L
#define ERROR_NOT_ENOUGH_MEMORY     8L

// Raw HDC font entry points, used with ::-qualified calls in format.cpp alongside
// the CDC members.
inline HFONT   CreateFontIndirect(const LOGFONT*) { return (HFONT)0; }
inline void*   SelectObject(HDC, void*) { return (void*)0; }
inline int     GetTextFace(HDC, int n, LPTSTR buf) { if (buf && n > 0) buf[0] = 0; return 0; }
inline UINT    GetTextCharset(HDC) { return ANSI_CHARSET; }
inline BOOL    GetTextMetrics(HDC, LPTEXTMETRIC tm) { if (tm) memset(tm, 0, sizeof(*tm)); return FALSE; }
inline BOOL    GetTextExtentPoint32(HDC, LPCTSTR, int, LPSIZE) { return FALSE; }

inline void SetLastError(DWORD) {}
inline DWORD GetLastError() { return 0; }

#define CFM_STRIKEOUT   0x00000008
#define CFE_STRIKEOUT   0x0008
#define CFM_OFFSET      0x10000000
inline HRSRC   FindResource(HINSTANCE, LPCTSTR, LPCTSTR) { return (HRSRC)0; }
inline HGLOBAL LoadResource(HINSTANCE, HRSRC) { return (HGLOBAL)0; }
inline void*   LockResource(HGLOBAL) { return (void*)0; }
inline BOOL    FreeResource(HGLOBAL) { return TRUE; }
inline DWORD   SizeofResource(HINSTANCE, HRSRC) { return 0; }

// Raw GDI blit entry points used directly (not via CDC) by dib.cpp's Draw path.
// Painting, so no-ops are safe until there is a window; see the class comment on
// CDC for why measurement is treated differently.
inline int SetDIBitsToDevice(HDC, int, int, DWORD, DWORD, int, int, UINT, UINT,
                             const void*, const BITMAPINFO*, UINT) { return 0; }
inline int StretchDIBits(HDC, int, int, int, int, int, int, int, int,
                         const void*, const BITMAPINFO*, UINT, DWORD) { return 0; }
inline int SetStretchBltMode(HDC, int) { return 0; }
inline int GetDeviceCaps(HDC, int) { return 0; }
#define COLORONCOLOR 3
#define HALFTONE     4
#define LOGPIXELSX   88
#define LOGPIXELSY   90

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
    CPen() {}
    CPen(int, int, COLORREF) {}
    BOOL CreatePen(int, int, COLORREF) { return TRUE; }
};

class CBrush : public CGdiObject {
public:
    CBrush() {}
    CBrush(COLORREF) {}
    BOOL CreateSolidBrush(COLORREF) { return TRUE; }
    BOOL CreateStockObject(int) { return TRUE; }
};

class CBitmap : public CGdiObject {
public:
    BOOL CreateCompatibleBitmap(CDC*, int, int) { return TRUE; }
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
    CDC() : m_hDC(0) {}
    HDC GetSafeHdc() const { return (HDC)m_hDC; }
    CFont* GetCurrentFont() const { return 0; }
    int GetTextFace(int n, LPTSTR buf) const { if (buf && n > 0) buf[0] = 0; return 0; }
    int GetTextFace(CString& s) const { s.Empty(); return 0; }
    CPalette* GetCurrentPalette() const { return 0; }
    virtual ~CDC() {}

    // -- mapping / state (safe no-ops) --
    int SetMapMode(int) { return MM_TEXT; }
    int GetMapMode() const { return MM_TWIPS; }
    int SetBkMode(int) { return TRANSPARENT; }
    COLORREF SetBkColor(COLORREF c) { return c; }
    COLORREF SetTextColor(COLORREF c) { return c; }
    int SetPolyFillMode(int) { return ALTERNATE; }
    BOOL SetViewportOrg(int, int) { return TRUE; }
    BOOL SetWindowOrg(int, int) { return TRUE; }
    BOOL SetWindowExt(int, int) { return TRUE; }
    BOOL SetViewportExt(int, int) { return TRUE; }
    BOOL IsPrinting() const { return FALSE; }
    int SaveDC() { return 1; }
    BOOL RestoreDC(int) { return TRUE; }

    CGdiObject* SelectObject(CGdiObject* p) { return p; }
    CFont* SelectObject(CFont* p) { return p; }
    CPen* SelectObject(CPen* p) { return p; }
    CBrush* SelectObject(CBrush* p) { return p; }
    CPalette* SelectPalette(CPalette* p, BOOL) { return p; }
    UINT RealizePalette() { return 0; }
    CGdiObject* SelectStockObject(int) { return 0; }

    // -- coordinate conversion --
    // Identity for now. These are only reached once a view exists, and a view
    // brings a real backend with it.
    void LPtoDP(POINT*, int = 1) const {}
    void DPtoLP(POINT*, int = 1) const {}
    void LPtoDP(RECT*) const {}
    void DPtoLP(RECT*) const {}

    // -- painting (no-ops) --
    BOOL MoveTo(int, int) { return TRUE; }
    BOOL LineTo(int, int) { return TRUE; }
    BOOL Polyline(const POINT*, int) { return TRUE; }
    BOOL Polygon(const POINT*, int) { return TRUE; }
    BOOL PolyBezier(const POINT*, int) { return TRUE; }
    BOOL PolyBezierTo(const POINT*, int) { return TRUE; }
    BOOL PolylineTo(const POINT*, int) { return TRUE; }
    BOOL ArcTo(int, int, int, int, int, int, int, int) { return TRUE; }
    BOOL LineTo(POINT p) { return LineTo((int)p.x, (int)p.y); }
    BOOL MoveTo(POINT p) { return MoveTo((int)p.x, (int)p.y); }
    BOOL Rectangle(int, int, int, int) { return TRUE; }
    BOOL Ellipse(int, int, int, int) { return TRUE; }
    BOOL Arc(int, int, int, int, int, int, int, int) { return TRUE; }
    BOOL BeginPath() { return TRUE; }
    BOOL EndPath() { return TRUE; }
    BOOL StrokeAndFillPath() { return TRUE; }
    BOOL StrokePath() { return TRUE; }
    BOOL FillPath() { return TRUE; }
    BOOL CloseFigure() { return TRUE; }
    BOOL TextOut(int, int, LPCTSTR, int) { return TRUE; }
    BOOL TextOut(int, int, const CString&) { return TRUE; }
    BOOL ExtTextOut(int, int, UINT, const RECT*, LPCTSTR, UINT, const int*) { return TRUE; }
    int DrawText(LPCTSTR, int, RECT*, UINT) { return 0; }
    BOOL BitBlt(int, int, int, int, CDC*, int, int, DWORD) { return TRUE; }
    BOOL StretchBlt(int, int, int, int, CDC*, int, int, int, int, DWORD) { return TRUE; }
    int SetStretchBltMode(int) { return 0; }
    BOOL PatBlt(int, int, int, int, DWORD) { return TRUE; }
    BOOL DrawIcon(int, int, HICON) { return TRUE; }
    BOOL DrawIcon(POINT, HICON) { return TRUE; }
    void FillRect(const RECT*, CBrush*) {}
    void FillSolidRect(const RECT*, COLORREF) {}
    void FillSolidRect(int, int, int, int, COLORREF) {}
    void Draw3dRect(const RECT*, COLORREF, COLORREF) {}
    void FrameRect(const RECT*, CBrush*) {}
    BOOL InvertRect(const RECT*) { return TRUE; }
    BOOL CreateCompatibleDC(CDC*) { return TRUE; }
    BOOL DeleteDC() { return TRUE; }
    BOOL SelectClipRgn(CRgn*) { return TRUE; }
    int IntersectClipRect(int, int, int, int) { return 0; }

    // -- MEASUREMENT: must never guess (see class comment) --
    SIZE GetTextExtent(LPCTSTR, int) const {
        NATIVE_GDI_UNIMPLEMENTED("CDC::GetTextExtent - must read the frozen glyph table");
    }
    SIZE GetTextExtent(const CString&) const {
        NATIVE_GDI_UNIMPLEMENTED("CDC::GetTextExtent(CString) - must read the frozen glyph table");
    }
    SIZE GetOutputTextExtent(LPCTSTR, int) const {
        NATIVE_GDI_UNIMPLEMENTED("CDC::GetOutputTextExtent");
    }
    BOOL GetTextMetrics(TEXTMETRIC*) const {
        NATIVE_GDI_UNIMPLEMENTED("CDC::GetTextMetrics");
    }
    BOOL GetCharWidth(UINT, UINT, int*) const {
        NATIVE_GDI_UNIMPLEMENTED("CDC::GetCharWidth");
    }
    int GetDeviceCaps(int) const {
        NATIVE_GDI_UNIMPLEMENTED("CDC::GetDeviceCaps");
    }
};

class CClientDC : public CDC {
public:
    CClientDC(CWnd*) {}
};
class CPaintDC : public CDC {
public:
    CPaintDC(CWnd*) {}
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
        m_fp = fopen(name, mode);
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
protected:
    FILE* m_fp;
};

class CMemFile : public CFile {};

#endif // NATIVE_SHIM_GDISHIM_H
