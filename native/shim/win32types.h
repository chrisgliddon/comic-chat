// win32types.h - Win32 scalar/struct types for the native macOS port.
//
// This is the bottom of the shim stack. Everything here exists so the original
// engine .cpp files compile unmodified under clang on arm64 macOS.
//
// ===========================================================================
// THE TRAP THAT MATTERS MOST: integer widths.
//
// Win32 is ILP32 - `long` is 4 bytes. macOS arm64 is LP64 - `long` is 8 bytes.
// The engine reads binary file structures straight into packed structs:
// avbfile.h has `typedef ULONG AVBINT32` inside `#pragma pack(push, 1)`, and
// AVATARBODYDATA/AVATARFACEDATA/AVATARTORSODATA are memcpy'd out of the stream.
// If ULONG were `unsigned long` here, every one of those structs would grow and
// every offset in every .avb would be read from the wrong bytes.
//
// So the fixed-width types below are deliberately NOT spelled `long`. They are
// int32_t/uint32_t regardless of host. Any future addition to this header must
// hold that line: on this platform `long` is never the right spelling for a
// Win32 32-bit type.
//
// The oracle is what makes this safe to get wrong once: a width mistake shows up
// as a mismatch against oracle/avb/*.golden.json rather than as silent corruption.
// ===========================================================================

#ifndef NATIVE_SHIM_WIN32TYPES_H
#define NATIVE_SHIM_WIN32TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// --- integer scalars -------------------------------------------------------
typedef int32_t             LONG;
typedef uint32_t            ULONG;
typedef uint32_t            DWORD;
typedef int32_t             INT;
typedef uint32_t            UINT;
typedef int16_t             SHORT;
typedef uint16_t            USHORT;
typedef uint16_t            WORD;
typedef uint8_t             BYTE;
typedef uint8_t             UCHAR;
typedef char                CHAR;
typedef int                 BOOL;

typedef DWORD*              LPDWORD;
typedef BYTE*               LPBYTE;
typedef WORD*               LPWORD;
typedef void*               LPVOID;
typedef void*               PVOID;
typedef BYTE*               PBYTE;
typedef const void*         LPCVOID;

// TCHAR is always narrow here: the engine is an MBCS build (it indexes bytes and
// calls strchr/strlen throughout), so widening it would be wrong, not merely
// unnecessary. See RULEBOOK on the CP-1252 byte orientation of strings.
typedef char                TCHAR;
typedef char*               LPSTR;
typedef char*               LPTSTR;
typedef const char*         LPCSTR;
typedef const char*         LPCTSTR;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

// _T/TEXT live here rather than only in tchar.h because several engine sources
// use _T() without including tchar.h - MSVC gets them transitively via windows.h.
#ifndef _T
#define _T(x)   x
#endif
#ifndef TEXT
#define TEXT(x) x
#endif

// Resource handles: dib.cpp references HRSRC/HGLOBAL for its LoadResource path.
// Opaque, and that path is unreachable natively (no PE resource section).
typedef void* HRSRC;
typedef void* HGLOBAL;
typedef void* HRGN;
typedef void* HMENU;
typedef void* HCURSOR;
typedef void* HICON;
typedef void* HBRUSH;
typedef void* HPEN;

// Handle types, declared here because the message/COM floor below references
// HWND. Repeated identically further down; an identical typedef redeclaration is
// legal C++, and keeping both makes each section readable on its own.
typedef void* HWND;
typedef void* HDC;
typedef void* HINSTANCE;
typedef void* HANDLE;

// Geometry, hoisted for the same reason: MSG embeds a POINT.
typedef struct tagPOINT { LONG x, y; } POINT, *LPPOINT;
typedef struct tagSIZE  { LONG cx, cy; } SIZE, *LPSIZE;
typedef struct tagRECT  { LONG left, top, right, bottom; } RECT, *LPRECT;

// Message-handler plumbing. afx_msg is purely a marker in MFC (it expands to
// nothing) but it must exist as a macro or every handler declaration is a syntax
// error - and handler declarations appear in headers that engine files include.
#define afx_msg
#define CALLBACK
#define WINAPI
#define APIENTRY
#define PASCAL
typedef intptr_t            LRESULT;
typedef uintptr_t           WPARAM;
typedef intptr_t            LPARAM;
typedef struct tagNMHDR { HWND hwndFrom; UINT idFrom; UINT code; } NMHDR, *LPNMHDR;
typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG;
typedef struct _SYSTEMTIME { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;
typedef struct _GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; } GUID, IID, CLSID;
typedef LONG HRESULT;
#define S_OK        ((HRESULT)0)
#define S_FALSE     ((HRESULT)1)
#define E_FAIL      ((HRESULT)0x80004005L)
#define NOERROR     0
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr)    ((HRESULT)(hr) < 0)
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#define MAX_PATH        260
#define _MAX_PATH       260
#define _MAX_FNAME      256
#define _MAX_EXT        256

// windows.h defines these as macros and the engine relies on it (bbox.cpp uses
// bare min/max on ints). Deliberately macros, not std::min/std::max: the engine
// mixes int/short/double operands freely, which the templates would reject.
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define LOBYTE(w)   ((BYTE)((w) & 0xFF))
#define HIBYTE(w)   ((BYTE)(((w) >> 8) & 0xFF))
#define LOWORD(l)   ((WORD)((l) & 0xFFFF))
#define HIWORD(l)   ((WORD)(((l) >> 16) & 0xFFFF))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | (((WORD)((BYTE)(b))) << 8)))

// --- colour ----------------------------------------------------------------
// COLORREF is 0x00BBGGRR: the LOW byte is RED. See RULEBOOK 15.4 - this is the
// single easiest thing to invert when moving to a platform whose native order is
// ARGB/RGBA, and inverting it channel-swaps every avatar.
typedef DWORD COLORREF;
#define RGB(r, g, b) \
    ((COLORREF)(((BYTE)(r)) | (((WORD)((BYTE)(g))) << 8) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) ((BYTE)((rgb) & 0xFF))
#define GetGValue(rgb) ((BYTE)((((WORD)(rgb)) >> 8) & 0xFF))
#define GetBValue(rgb) ((BYTE)(((rgb) >> 16) & 0xFF))

// --- DIB structures --------------------------------------------------------
// Layout must match Win32 exactly: these are read from and written to .avb/.bmp
// bytes. RGBQUAD is B,G,R,reserved - the reverse of COLORREF's channel order.
#pragma pack(push, 1)
typedef struct tagRGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD, *LPRGBQUAD;

typedef struct tagRGBTRIPLE {
    BYTE rgbtBlue;
    BYTE rgbtGreen;
    BYTE rgbtRed;
} RGBTRIPLE;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG  biWidth;
    LONG  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagBITMAPCOREHEADER {
    DWORD bcSize;
    WORD  bcWidth;
    WORD  bcHeight;
    WORD  bcPlanes;
    WORD  bcBitCount;
} BITMAPCOREHEADER, *LPBITMAPCOREHEADER;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO, *PBITMAPINFO;

typedef struct tagBITMAPFILEHEADER {
    WORD  bfType;
    DWORD bfSize;
    WORD  bfReserved1;
    WORD  bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER;
#pragma pack(pop)

#define BI_RGB        0
#define BI_RLE8       1
#define BI_RLE4       2
#define BI_BITFIELDS  3

// --- misc Win32 spellings the engine uses ---------------------------------
#define ZeroMemory(p, n)      memset((p), 0, (n))
#define CopyMemory(d, s, n)   memcpy((d), (s), (n))
#define FillMemory(p, n, b)   memset((p), (b), (n))

#define SEEK_CUR_WIN32 1

// GDI handle types appear in signatures the port does not call into; they are
// opaque here so headers parse without pulling in a graphics backend.
typedef void* HDC;
typedef void* HBITMAP;
typedef void* HPALETTE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HANDLE;
typedef void* HFONT;

// Forward declarations for the MFC graphics classes. pe.h and friends take CDC*
// purely as an opaque parameter, so the engine's headers parse with just these -
// no Core Graphics backend is needed until something actually draws.
class CDC;
class CPalette;
class CFont;
class CBitmap;
class CWnd;

#endif // NATIVE_SHIM_WIN32TYPES_H
