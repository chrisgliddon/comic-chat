// winsock.h - Winsock 1.1 over BSD sockets.
//
// Unlike most of this shim, this is not throwaway scaffolding. Winsock was
// modelled on BSD sockets, so the mapping is thin and mostly real: `SOCKET`
// becomes an int fd, and send/recv/connect/bind are the same calls. When the
// native app grows real networking, ircsock.cpp should work against this rather
// than being rewritten.
//
// The differences that actually bite, all handled below:
//
//  * Winsock's INVALID_SOCKET is ~0 and SOCKET_ERROR is -1; BSD uses -1 for both.
//  * closesocket() vs close(); ioctlsocket() vs ioctl/fcntl.
//  * WSAGetLastError() reads a per-thread Winsock error, not errno. Mapped to
//    errno, which is close enough for the codes the engine tests.
//  * Winsock needs WSAStartup/WSACleanup; BSD needs neither, so they succeed
//    trivially rather than being stripped from call sites.
//  * The engine's asynchronous model (WSAAsyncSelect posting window messages) has
//    NO BSD equivalent and is deliberately not emulated here. That is a real
//    design decision for the app layer - a run loop source or a reader thread -
//    not something a header can paper over, so the async entry points are absent
//    rather than stubbed. Anything that needs them will fail to compile, which is
//    the honest outcome.

#ifndef NATIVE_SHIM_WINSOCK_H
#define NATIVE_SHIM_WINSOCK_H

#include "win32types.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>

typedef int SOCKET;

#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)

typedef struct sockaddr      SOCKADDR, *PSOCKADDR, *LPSOCKADDR;
typedef struct sockaddr_in   SOCKADDR_IN, *PSOCKADDR_IN, *LPSOCKADDR_IN;
typedef struct in_addr       IN_ADDR, *PIN_ADDR, *LPIN_ADDR;
typedef struct hostent       HOSTENT, *PHOSTENT, *LPHOSTENT;
typedef struct servent       SERVENT, *PSERVENT, *LPSERVENT;
typedef struct timeval       TIMEVAL, *PTIMEVAL, *LPTIMEVAL;
typedef fd_set               FD_SET_T;

inline int closesocket(SOCKET s) { return close(s); }
inline int ioctlsocket(SOCKET s, long cmd, unsigned long* argp) {
    return ioctl(s, (unsigned long)cmd, argp);
}

// WSAStartup/WSACleanup have no BSD counterpart; succeeding keeps the engine's
// init sequence intact instead of requiring edits at the call sites.
typedef struct WSAData {
    WORD wVersion, wHighVersion;
    char szDescription[257], szSystemStatus[129];
    unsigned short iMaxSockets, iMaxUdpDg;
    char* lpVendorInfo;
} WSADATA, *LPWSADATA;

inline int WSAStartup(WORD, LPWSADATA d) {
    if (d) { memset(d, 0, sizeof(*d)); d->wVersion = 0x0101; }
    return 0;
}
inline int WSACleanup() { return 0; }
inline int WSAGetLastError() { return errno; }
inline void WSASetLastError(int e) { errno = e; }

// Winsock error names the engine compares against.
#define WSAEWOULDBLOCK      EWOULDBLOCK
#define WSAEINPROGRESS      EINPROGRESS
#define WSAEALREADY         EALREADY
#define WSAENOTSOCK         ENOTSOCK
#define WSAEADDRINUSE       EADDRINUSE
#define WSAECONNREFUSED     ECONNREFUSED
#define WSAECONNRESET       ECONNRESET
#define WSAECONNABORTED     ECONNABORTED
#define WSAETIMEDOUT        ETIMEDOUT
#define WSAENETUNREACH      ENETUNREACH
#define WSAEHOSTUNREACH     EHOSTUNREACH
#define WSAEISCONN          EISCONN
#define WSAENOTCONN         ENOTCONN
#define WSAEINTR            EINTR
#define WSAEMFILE           EMFILE

#define WSAVERSION          0x0101
#define MAKEWORD_WS(a, b)   MAKEWORD(a, b)

#endif // NATIVE_SHIM_WINSOCK_H
