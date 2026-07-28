// wininet.h - stand-in for <Wininet.H>, which format.cpp includes for the URL
// handling types. The native build does no WinInet networking; these exist so the
// declarations parse.
//
// Note the original spelling is <Wininet.H> with capitals. That resolves here
// because the default macOS filesystem is case-insensitive; on a case-sensitive
// volume this file would need the capitalised name too.

#ifndef NATIVE_SHIM_WININET_H
#define NATIVE_SHIM_WININET_H

#include "win32types.h"

// Declared before use below; the real SetLastError lives in gdishim.h, which this
// header must not depend on.
static inline void SetLastError_Stub() {}

typedef void* HINTERNET;
typedef DWORD INTERNET_PORT;

#define INTERNET_MAX_URL_LENGTH     2084
#define INTERNET_MAX_HOST_NAME_LENGTH 256
#define INTERNET_MAX_PATH_LENGTH    2048
#define INTERNET_MAX_SCHEME_LENGTH  32

#define INTERNET_SCHEME_UNKNOWN     (-2)
#define INTERNET_SCHEME_DEFAULT     0
#define INTERNET_SCHEME_FTP         1
#define INTERNET_SCHEME_GOPHER      2
#define INTERNET_SCHEME_HTTP        3
#define INTERNET_SCHEME_HTTPS       4
#define INTERNET_SCHEME_FILE        5
#define INTERNET_SCHEME_MAILTO      6

typedef struct {
    DWORD    dwStructSize;
    LPSTR    lpszScheme;
    DWORD    dwSchemeLength;
    int      nScheme;
    LPSTR    lpszHostName;
    DWORD    dwHostNameLength;
    INTERNET_PORT nPort;
    LPSTR    lpszUserName;
    DWORD    dwUserNameLength;
    LPSTR    lpszPassword;
    DWORD    dwPasswordLength;
    LPSTR    lpszUrlPath;
    DWORD    dwUrlPathLength;
    LPSTR    lpszExtraInfo;
    DWORD    dwExtraInfoLength;
} URL_COMPONENTS, *LPURL_COMPONENTS;

// InternetCanonicalizeUrl - urlutil.cpp uses it to normalise a detected URL before
// launching it. Reports the insufficient-buffer failure that its caller already
// handles, rather than attempting a canonicalisation: URL escaping rules are exactly
// the kind of thing that looks simple and is not, and getting them subtly different
// from WinInet would change which URLs the engine recognises. If the native app ever
// needs this, NSURL is the right implementation - not a hand-rolled escaper here.
#ifndef ERROR_INSUFFICIENT_BUFFER
#define ERROR_INSUFFICIENT_BUFFER  122L
#endif
#define ICU_NO_ENCODE       0x20000000
#define ICU_DECODE          0x10000000
#define ICU_NO_META         0x08000000
#define ICU_ENCODE_SPACES_ONLY 0x04000000
#define ICU_BROWSER_MODE    0x02000000
#define ICU_ESCAPE          0x80000000
#ifndef ERROR_BAD_PATHNAME
#define ERROR_BAD_PATHNAME  161L
#endif
#define ERROR_INTERNET_BASE                 12000
#define ERROR_INTERNET_INVALID_URL          (ERROR_INTERNET_BASE + 5)
#define ERROR_INTERNET_UNRECOGNIZED_SCHEME  (ERROR_INTERNET_BASE + 6)
#define ERROR_INTERNET_NAME_NOT_RESOLVED    (ERROR_INTERNET_BASE + 7)

static inline BOOL InternetCanonicalizeUrl(LPCSTR, LPSTR, LPDWORD, DWORD) {
    SetLastError_Stub();
    return FALSE;
}

#endif // NATIVE_SHIM_WININET_H
