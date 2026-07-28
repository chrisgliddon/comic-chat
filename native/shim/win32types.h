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
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

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
// bodycam.cpp casts through (VOID**) when calling CreateDIBSection.
#define VOID                void
typedef BYTE*               PBYTE;
typedef const void*         LPCVOID;
typedef int*                PINT;
typedef UINT*               PUINT;
typedef LONG*               PLONG;
typedef DWORD*              PDWORD;
typedef WORD*               PWORD;
typedef BOOL*               PBOOL;
typedef SHORT*              PSHORT;
typedef char*               PCHAR;
typedef char*               PSTR;
typedef int*                LPINT;
typedef long*               LPLONG;
typedef BOOL*               LPBOOL;
// HINTERNET belongs to wininet.h, but webreq.h uses it while being reached from
// chat.h without that include - same include-order shortfall as CCNotif. Declared
// at the type floor so both spellings agree.
typedef void*               HINTERNET;
typedef const char*         PCSTR;
typedef long                LONG_PTR;
typedef unsigned long       ULONG_PTR;
typedef ULONG_PTR           DWORD_PTR;
typedef uintptr_t           UINT_PTR;
typedef intptr_t            INT_PTR;

// TCHAR is always narrow here: the engine is an MBCS build (it indexes bytes and
// calls strchr/strlen throughout), so widening it would be wrong, not merely
// unnecessary. See RULEBOOK on the CP-1252 byte orientation of strings.
typedef char                TCHAR;
typedef char*               LPSTR;
typedef char*               LPTSTR;
typedef const char*         LPCSTR;
typedef const char*         LPCTSTR;
typedef TCHAR               _TCHAR;
typedef float               FLOAT;
typedef int (*FARPROC)();
typedef void*               HHOOK;

// GUID is hoisted here because the OLE typedefs below reference it through
// REFIID. Defined once; the COM floor further down no longer redeclares it
// (a struct redefinition, unlike a repeated typedef, is an error).
typedef struct _GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; } GUID, IID, CLSID;
// GUID_NULL: the harness stubs the COM type-library IIDs with it.
#ifdef __cplusplus
static const GUID GUID_NULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
#endif

// WCHAR/BSTR/VARIANT appear in the OLE automation declarations. The native build
// drops OLE entirely, but the types are referenced from headers the engine core
// reaches, so they need to exist. wchar_t is 4 bytes on macOS vs 2 on Win32 -
// irrelevant here because nothing native marshals these, but it would matter
// immediately if any OLE path were revived.
typedef uint16_t            WCHAR;
typedef WCHAR*              LPWSTR;
typedef const WCHAR*        LPCWSTR;
typedef LPWSTR              BSTR;
typedef WCHAR               OLECHAR;
typedef LPWSTR              LPOLESTR;
typedef const WCHAR*        LPCOLESTR;
typedef LONG                SCODE;
typedef short               VARIANT_BOOL;
typedef double              DATE;
typedef struct tagVARIANT { WORD vt; WORD r1, r2, r3; union { LONG lVal; double dblVal; BSTR bstrVal; void* byref; }; } VARIANT, VARIANTARG;
typedef VARIANT*            LPVARIANT;
typedef struct tagDISPPARAMS { VARIANTARG* rgvarg; LONG* rgdispidNamedArgs; UINT cArgs, cNamedArgs; } DISPPARAMS;
typedef struct tagEXCEPINFO { WORD wCode, wReserved; BSTR bstrSource, bstrDescription, bstrHelpFile; DWORD dwHelpContext; void* pvReserved; void* pfnDeferredFillIn; SCODE scode; } EXCEPINFO;
typedef LONG                DISPID;

// C++ only from here to the end of the OLE block: REFIID is a reference type and
// IDispatch uses inheritance. The engine's three C files (intl.c, jis2sjis.c,
// sjis2jis.c) include this header via stdafx.h and need none of it.
#ifdef __cplusplus
typedef const GUID&         REFIID;
typedef const GUID&         REFCLSID;
// IUnknown/IDispatch must be real (if empty) types, not void: icchat.h declares
// its automation interfaces as deriving from them, and a base specifier needs a
// class. No method bodies - nothing native implements COM.
struct IUnknown {};
struct IDispatch : public IUnknown {};
typedef void*               LPUNKNOWN;
typedef void*               LPDISPATCH;
typedef void*               LPSTREAM;
typedef void*               LPSTORAGE;
typedef void*               LPMONIKER;
typedef void*               LPBC;
typedef void*               LPOLEOBJECT;
#endif // __cplusplus (OLE block)

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
typedef const RECT*         LPCRECT;
typedef const POINT*        LPCPOINT;
typedef const SIZE*         LPCSIZE;

// Message-handler plumbing. afx_msg is purely a marker in MFC (it expands to
// nothing) but it must exist as a macro or every handler declaration is a syntax
// error - and handler declarations appear in headers that engine files include.
// The segmented-memory decorations. Empty in Win32 and here; they survive in the
// engine's declarations (whisprbx.h has `MINMAXINFO FAR* lpMMI`) because MFC's
// headers still defined them.
#ifndef FAR
#define FAR
#endif
#ifndef NEAR
#define NEAR
#endif
#define _far
#define _near
#define huge

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
typedef LONG HRESULT;
#define S_OK        ((HRESULT)0)
#define S_FALSE     ((HRESULT)1)
#define E_FAIL      ((HRESULT)0x80004005L)
// E_ABORT is how HrGenerateAndSendAuthMsg reports "the user cancelled this
// authentication package"; ircsock.cpp compares against it to move to the next package.
#define E_ABORT     ((HRESULT)0x80004004L)
#define NOERROR     0
// NO_ERROR is the Win32 (not COM) success code, 0. ircsock.cpp tests the SSPI status
// against it. Distinct spelling from NOERROR above, and both appear in the engine.
#define NO_ERROR    0L
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr)    ((HRESULT)(hr) < 0)
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

// CRITICAL_SECTION over pthreads rather than no-ops. mcithrd.cpp and the socket
// layer do run background threads in the original, so a no-op lock would be a
// real race rather than dead scaffolding - cheap to do correctly, so it is.
#include <pthread.h>
typedef struct { pthread_mutex_t mtx; int initialised; } CRITICAL_SECTION, *LPCRITICAL_SECTION;
static inline void InitializeCriticalSection(LPCRITICAL_SECTION cs) {
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    // Win32 critical sections are recursive; a non-recursive mutex here would
    // deadlock any code that re-enters, which MFC-era code does freely.
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&cs->mtx, &a);
    pthread_mutexattr_destroy(&a);
    cs->initialised = 1;
}
static inline void EnterCriticalSection(LPCRITICAL_SECTION cs) { if (cs->initialised) pthread_mutex_lock(&cs->mtx); }
static inline void LeaveCriticalSection(LPCRITICAL_SECTION cs) { if (cs->initialised) pthread_mutex_unlock(&cs->mtx); }
static inline void DeleteCriticalSection(LPCRITICAL_SECTION cs) { if (cs->initialised) { pthread_mutex_destroy(&cs->mtx); cs->initialised = 0; } }

// VirtualProtect: dib.cpp pokes page permissions on its resource path, which is
// unreachable natively. Reporting success keeps the caller's error handling quiet
// on a path that never runs.
#define PAGE_READWRITE 0x04
// Module / version APIs. ircsock.cpp dynamically loads the SSPI security DLL and
// branches on the Windows version. Everything here reports failure or a neutral
// value: there is no security DLL to load, and the native client will do plain IRC
// rather than Windows-integrated authentication.
typedef struct _OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId;
    char  szCSDVersion[128];
} OSVERSIONINFOA, OSVERSIONINFO, *LPOSVERSIONINFO;
#define VER_PLATFORM_WIN32_NT       2
#define VER_PLATFORM_WIN32_WINDOWS  1

static inline BOOL GetVersionEx(LPOSVERSIONINFO v) {
    if (!v) return FALSE;
    DWORD keep = v->dwOSVersionInfoSize;
    memset(v, 0, sizeof(*v));
    v->dwOSVersionInfoSize = keep;
    // Reported as NT so any "is this 9x" branch takes the modern path.
    v->dwPlatformId = VER_PLATFORM_WIN32_NT;
    v->dwMajorVersion = 10;
    return TRUE;
}
static inline HINSTANCE LoadLibrary(LPCSTR) { return (HINSTANCE)0; }
static inline BOOL FreeLibrary(HINSTANCE) { return TRUE; }
static inline FARPROC GetProcAddress(HINSTANCE, LPCSTR) { return (FARPROC)0; }
#ifndef STDAPI
#define STDAPI          extern "C" HRESULT
#define STDAPI_(t)      extern "C" t
#define STDMETHODCALLTYPE
#endif
#define E_OUTOFMEMORY   ((HRESULT)0x8007000EL)
#define E_INVALIDARG    ((HRESULT)0x80070057L)
#define E_NOTIMPL       ((HRESULT)0x80004001L)

// DVASPECT - the OLE presentation aspect, named in ChatItem.h.
typedef enum tagDVASPECT { DVASPECT_CONTENT = 1, DVASPECT_THUMBNAIL = 2,
                           DVASPECT_ICON = 4, DVASPECT_DOCPRINT = 8 } DVASPECT;

static inline BOOL VirtualProtect(void*, size_t, DWORD, PDWORD old) { if (old) *old = PAGE_READWRITE; return TRUE; }

#define MAX_PATH        260
#define _MAX_PATH       260
#define _MAX_FNAME      256
#define _MAX_EXT        256
#define _MAX_DRIVE      3
#define _MAX_DIR        256

// NOTE: min/max are NOT defined here. They are defined at the END of stdafx.h,
// after every C++ standard header, because libc++ #undefs them - see the comment
// there. Defining them at this point silently loses them on some toolchains.

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
// lstr* are the Win32 string entry points; the engine mixes them with str*.
#define lstrcpy     strcpy
#define lstrcpyn    strncpy
#define lstrcat     strcat
#define lstrlen     strlen
#define lstrcmp     strcmp
#define lstrcmpi    strcasecmp
// __T is the inner half of MSVC's _T macro; avbfile.cpp reaches it directly.
#ifndef __T
#define __T(x)      x
#endif

// --- last-error -------------------------------------------------------------------
// Real, not a stub. Two live call sites branch on the value:
//   core/ccommon.cpp    GetLastError() == ERROR_INSUFFICIENT_BUFFER distinguishes "the
//                       output buffer was too small" (give up) from any other failure
//                       (fall through to LUnchanged and pass the text through).
//   chatsrv.cpp:1778    GetLastError() == WSAEWOULDBLOCK is how a non-blocking connect
//                       reports success, so a hardcoded 0 would read as a real failure
//                       on every connection attempt.
// A single instance shared across translation units: `inline` (external linkage) with a
// function-local static is the header-only way to get that in C++. Not thread-local -
// Win32's is per-thread, but the engine only touches it on the main thread, and a
// silently per-thread value would be harder to reason about than a shared one.
#define ERROR_SUCCESS               0L
#define ERROR_INVALID_PARAMETER     87L
#define ERROR_FILE_NOT_FOUND        2L
#define ERROR_NOT_ENOUGH_MEMORY     8L
#define ERROR_INSUFFICIENT_BUFFER   122L

#ifdef __cplusplus
inline DWORD& Win32LastErrorSlot() { static DWORD e = 0; return e; }
inline void  SetLastError(DWORD e) { Win32LastErrorSlot() = e; }
inline DWORD GetLastError()        { return Win32LastErrorSlot(); }
#endif

// wsprintf is USER32's sprintf. Real, not a stub: ircproto.cpp formats the quoted
// nickname with it. Deliberately a function rather than `#define wsprintf sprintf`,
// because the engine also takes its address in a couple of table initialisers, and a
// macro would not survive that.
static inline int wvsprintf(LPSTR dst, LPCSTR fmt, va_list ap) {
    return vsprintf(dst, fmt, ap);
}
static inline int wsprintf(LPSTR dst, LPCSTR fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsprintf(dst, fmt, ap);
    va_end(ap);
    return n;
}

// File-attribute queries. The engine uses GetFileAttributes == -1 as its "does
// this file exist" test (LoadAvatarInfo does exactly that), so this must answer
// truthfully rather than stub - avatar loading depends on it.
#include <sys/stat.h>
#define INVALID_FILE_ATTRIBUTES  ((DWORD)-1)
#define FILE_ATTRIBUTE_READONLY  0x00000001
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL    0x00000080
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100
#define FILE_ATTRIBUTE_HIDDEN    0x00000002
#define CREATE_NEW               1
#define CREATE_ALWAYS            2
#define OPEN_EXISTING            3
#define OPEN_ALWAYS              4
#define GENERIC_READ             0x80000000
#define GENERIC_WRITE            0x40000000
#define FILE_SHARE_READ          0x00000001
// CreateFile / CloseHandle / FindExecutable: urlutil.cpp's temp-file and
// file-association probing. Report failure - there is no file association database
// here, and NSWorkspace is the right answer if the native app ever needs one.
static inline HANDLE CreateFile(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) {
    return INVALID_HANDLE_VALUE;
}
static inline BOOL CloseHandle(HANDLE) { return TRUE; }
static inline HINSTANCE FindExecutable(LPCSTR, LPCSTR, LPSTR buf) {
    if (buf) buf[0] = 0;
    return (HINSTANCE)(uintptr_t)31;   // < 32 == not found, per the Win32 contract
}
// --- path separator translation ---------------------------------------------------
// Win32's separator is '\' and the engine formats paths with it:
//     path.Format("%s\\%s.avb", theApp.GetAvatarDir(), avName);   (avatario.cpp:20)
// There are nine such sites (avatar, backdrop, favourites, .ccr, screenshots). Handling
// it HERE rather than editing them matters for two reasons:
//
//   * Not every backslash-joined string is a filesystem path. chatsrv.cpp:878 builds a
//     REGISTRY key the same way, where '\' is correct and must not be touched. A blanket
//     source edit would corrupt it.
//   * Translating at the boundary keeps the engine sources byte-identical to the Windows
//     build, so the Windows oracle cannot be broken by a portability edit that only the
//     macOS build needs. Backslash-as-separator is a platform semantic, which makes it
//     the shim's job by definition.
//
// Applied at every point where a path reaches the OS: GetFileAttributes and DeleteFile
// here, CFile::Open in gdishim.h, and the directory walk in io.h.
#define NATIVE_PATH_MAX 1024
static inline const char* NativePath(const char* p, char* buf, size_t n) {
    if (!p) return p;
    size_t i = 0;
    for (; p[i] && i + 1 < n; i++) buf[i] = (p[i] == '\\') ? '/' : p[i];
    buf[i] = '\0';
    return buf;
}

// Not every path reaches the OS through a shim function. CAvatarFileStream (avbfile.h)
// holds a raw FILE* and calls fopen itself, so CFile::Open's translation never sees the
// avatar file - which is exactly what made LoadAvatar fail while everything else worked.
// Redirecting fopen catches that and any other direct use in the engine.
//
// No recursion: the macro is defined AFTER this function's body has been preprocessed,
// so the fopen call inside it is still the real one.
static inline FILE* NativeFopen(const char* path, const char* mode) {
    char buf[NATIVE_PATH_MAX];
    if (!path) return (FILE*)0;
    return fopen(NativePath(path, buf, sizeof(buf)), mode);
}
#define fopen(p, m) NativeFopen((p), (m))

static inline DWORD GetFileAttributes(const char* path) {
    struct stat st;
    char pbuf[NATIVE_PATH_MAX];
    if (!path) return INVALID_FILE_ATTRIBUTES;
    if (stat(NativePath(path, pbuf, sizeof(pbuf)), &st) != 0) return INVALID_FILE_ATTRIBUTES;
    DWORD a = FILE_ATTRIBUTE_NORMAL;
    if (S_ISDIR(st.st_mode)) a |= FILE_ATTRIBUTE_DIRECTORY;
    if (!(st.st_mode & S_IWUSR)) a |= FILE_ATTRIBUTE_READONLY;
    return a;
}
static inline BOOL DeleteFile(const char* path) {
    char pbuf[NATIVE_PATH_MAX];
    return path && unlink(NativePath(path, pbuf, sizeof(pbuf))) == 0;
}

// _splitpath / _makepath - MSVC path helpers. Real implementations: avatar.cpp
// uses _splitpath to derive an avatar name from a filename, so a stub would break
// avatar enumeration.
static inline void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext) {
    if (drive) drive[0] = 0;
    if (dir) dir[0] = 0;
    if (fname) fname[0] = 0;
    if (ext) ext[0] = 0;
    if (!path) return;
    const char* slash = strrchr(path, '/');
    const char* bslash = strrchr(path, '\\');
    if (bslash > slash) slash = bslash;
    const char* base = slash ? slash + 1 : path;
    if (dir && slash) {
        size_t n = (size_t)(base - path);
        memcpy(dir, path, n); dir[n] = 0;
    }
    const char* dot = strrchr(base, '.');
    size_t stem = dot ? (size_t)(dot - base) : strlen(base);
    if (fname) { memcpy(fname, base, stem); fname[stem] = 0; }
    if (ext && dot) strcpy(ext, dot);
}

static inline void _makepath(char* path, const char* drive, const char* dir,
                      const char* fname, const char* ext) {
    if (!path) return;
    path[0] = 0;
    if (drive && *drive) strcat(path, drive);
    if (dir && *dir) strcat(path, dir);
    if (fname && *fname) strcat(path, fname);
    if (ext && *ext) strcat(path, ext);
}

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
//
// C++ only: the engine's C files reach this header through stdafx.h and have no use
// for MFC class names.
#ifdef __cplusplus
class CDC;
class CPalette;
class CFont;
class CBitmap;
class CWnd;
#endif // __cplusplus

// ---------------------------------------------------------------------------
// Process/module entry points the oracle harness itself uses for AfxWinInit. Inert:
// there is no module handle and no Win32 command line, and AfxWinInit is a no-op in
// the shim, so nothing reads either value.
// ---------------------------------------------------------------------------
#ifdef __cplusplus
static inline HINSTANCE GetModuleHandle(LPCSTR) { return (HINSTANCE)0; }
static inline LPSTR GetCommandLine() { return (LPSTR)""; }
static inline DWORD GetModuleFileName(HINSTANCE, LPSTR buf, DWORD n) {
    if (buf && n) buf[0] = 0;
    return 0;
}
static inline DWORD GetCurrentThreadId() { return 0; }
static inline DWORD GetCurrentProcessId() { return 0; }
#endif

#endif // NATIVE_SHIM_WIN32TYPES_H
