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
    void FillRect(const RECT*, CBrush*) {}
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
