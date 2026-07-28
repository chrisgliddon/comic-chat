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

#endif // NATIVE_SHIM_WININET_H
