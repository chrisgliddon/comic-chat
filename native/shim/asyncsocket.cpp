// asyncsocket.cpp - CAsyncSocket over BSD sockets and a CFRunLoop source.
//
// This is what makes real IRC possible, and the reason it can be small is a genuine
// correspondence rather than a workaround:
//
//   MFC's CAsyncSocket is asynchronous through WSAAsyncSelect, which asks Winsock to POST A
//   WINDOW MESSAGE when a socket becomes readable, writable or closed. The message pump then
//   calls OnReceive/OnConnect/OnClose on the UI thread. Nothing blocks, and because the
//   callbacks arrive on the same thread as everything else, the engine needs no locking.
//
//   A CFSocket attached to the main run loop has precisely those semantics: the run loop
//   watches the descriptor and invokes the callback on the main thread.
//
// So WSAAsyncSelect's absence from BSD sockets - noted in winsock.h as something a header
// could not paper over - is answered here at the right layer, by the platform's own event
// source. No reader thread, and therefore no locking around CChatDoc.
//
// Connect deserves a note. It is non-blocking, so connect(2) returns EINPROGRESS and the
// result arrives later as writability. MFC reports that as FALSE with WSAEWOULDBLOCK, and
// chatsrv.cpp:1778 depends on the distinction:
//
//     BOOL b = CAsyncSocket::Connect(...) || GetLastError() == WSAEWOULDBLOCK;
//
// so an in-progress connect MUST set that error or a perfectly good connection reads as a
// failure.

#include "stdafx.h"

#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

// The FD_* event bits WSAAsyncSelect uses. AsyncSelect callers pass these.
#ifndef FD_READ
#define FD_READ     0x01
#define FD_WRITE    0x02
#define FD_OOB      0x04
#define FD_ACCEPT   0x08
#define FD_CONNECT  0x10
#define FD_CLOSE    0x20
#endif

static void NativeAsyncSocketCallback(CFSocketRef s, CFSocketCallBackType type,
                                      CFDataRef addr, const void* data, void* info);

namespace {

// COMIC_CHAT_SOCKET_TRACE=1 prints every registration and callback. Socket lifecycle bugs are
// invisible from the outside - "status stays at connecting" is the same symptom whether the
// callback never fired, fired with an error, or fired on a socket that had been re-registered
// out from under it.
bool Trace() {
    static int on = -1;
    if (on < 0) on = getenv("COMIC_CHAT_SOCKET_TRACE") ? 1 : 0;
    return on != 0;
}

bool SetNonBlocking(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    return fl >= 0 && fcntl(fd, F_SETFL, fl | O_NONBLOCK) == 0;
}

} // namespace

CAsyncSocket::CAsyncSocket()
    : m_hSocket(-1), m_cfSocket(0), m_rlSource(0),
      m_events(FD_READ | FD_WRITE | FD_CONNECT | FD_CLOSE), m_connecting(0) {}

CAsyncSocket::~CAsyncSocket() { Close(); }

void CAsyncSocket::Unregister() {
    if (m_rlSource) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), (CFRunLoopSourceRef)m_rlSource,
                              kCFRunLoopCommonModes);
        CFRelease((CFRunLoopSourceRef)m_rlSource);
        m_rlSource = 0;
    }
    if (m_cfSocket) {
        // Invalidate before releasing, or the run loop can call back into a dead object.
        CFSocketInvalidate((CFSocketRef)m_cfSocket);
        CFRelease((CFSocketRef)m_cfSocket);
        m_cfSocket = 0;
    }
}

void CAsyncSocket::Register(long lEvent) {
    Unregister();
    if (m_hSocket < 0) return;
    m_events = lEvent ? lEvent : m_events;

    CFOptionFlags cb = 0;
    if (m_events & (FD_READ | FD_CLOSE)) cb |= kCFSocketReadCallBack;
    if (m_events & FD_CONNECT)           cb |= kCFSocketConnectCallBack;
    if (m_events & FD_ACCEPT)            cb |= kCFSocketAcceptCallBack;
    if (cb == 0) return;

    CFSocketContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.info = this;

    // kCFSocketCloseOnInvalidate is deliberately CLEARED below: Close() owns the descriptor,
    // and letting CFSocket close it too produces a double close that can land on an unrelated
    // descriptor opened in between - the classic version of this bug.
    CFSocketRef sock = CFSocketCreateWithNative(NULL, (CFSocketNativeHandle)m_hSocket,
                                               cb, NativeAsyncSocketCallback, &ctx);
    if (!sock) return;
    CFSocketSetSocketFlags(sock, CFSocketGetSocketFlags(sock) & ~kCFSocketCloseOnInvalidate);

    CFRunLoopSourceRef src = CFSocketCreateRunLoopSource(NULL, sock, 0);
    if (!src) { CFRelease(sock); return; }
    CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopCommonModes);

    m_cfSocket = sock;
    m_rlSource = src;
    if (Trace())
        fprintf(stderr, "[sock %d] registered cb=0x%lx (events=0x%lx)\n",
                m_hSocket, (unsigned long)cb, (unsigned long)m_events);
}

BOOL CAsyncSocket::Create(UINT nSocketPort, int nSocketType, long lEvent,
                          LPCTSTR lpszSocketAddress) {
    Close();
    m_hSocket = socket(AF_INET, nSocketType == 2 /*SOCK_DGRAM*/ ? SOCK_DGRAM : SOCK_STREAM, 0);
    if (m_hSocket < 0) { SetLastError((DWORD)errno); return FALSE; }

    int one = 1;
    setsockopt(m_hSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    // Without this, writing to a peer that has gone away raises SIGPIPE and kills the app
    // instead of returning an error the engine can report.
    setsockopt(m_hSocket, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));

    if (nSocketPort != 0 || lpszSocketAddress) {
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons((unsigned short)nSocketPort);
        a.sin_addr.s_addr = lpszSocketAddress ? inet_addr(lpszSocketAddress) : htonl(INADDR_ANY);
        if (bind(m_hSocket, (struct sockaddr*)&a, sizeof(a)) != 0) {
            SetLastError((DWORD)errno);
            close(m_hSocket);
            m_hSocket = -1;
            return FALSE;
        }
    }

    if (!SetNonBlocking(m_hSocket)) { SetLastError((DWORD)errno); }
    Register(lEvent);
    return TRUE;
}

BOOL CAsyncSocket::Connect(LPCTSTR lpszHostAddress, UINT nHostPort) {
    if (!lpszHostAddress) return FALSE;
    if (m_hSocket < 0 && !Create()) return FALSE;

    // Resolution is synchronous. getaddrinfo can block for seconds on a bad DNS server, which
    // would freeze the UI; doing it off-thread is a real improvement but a separate change,
    // and doing it wrong here would be worse than doing it plainly.
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port[16];
    snprintf(port, sizeof(port), "%u", nHostPort);

    struct addrinfo* res = 0;
    if (getaddrinfo(lpszHostAddress, port, &hints, &res) != 0 || !res) {
        SetLastError((DWORD)WSAEHOSTUNREACH);
        return FALSE;
    }
    BOOL ok = Connect((const SOCKADDR*)res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    return ok;
}

BOOL CAsyncSocket::Connect(const SOCKADDR* pSockAddr, int nSockAddrLen) {
    if (!pSockAddr) return FALSE;
    if (m_hSocket < 0 && !Create()) return FALSE;

    if (connect(m_hSocket, (const struct sockaddr*)pSockAddr, (socklen_t)nSockAddrLen) == 0) {
        m_connecting = 0;
        OnConnect(0);
        return TRUE;
    }
    if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
        // In progress, not failed. The error code is load-bearing - see the header comment.
        m_connecting = 1;
        SetLastError((DWORD)WSAEWOULDBLOCK);
        Register(m_events | FD_CONNECT);
        return FALSE;
    }
    SetLastError((DWORD)errno);
    return FALSE;
}

int CAsyncSocket::Send(const void* buf, int len, int flags) {
    if (m_hSocket < 0 || !buf || len <= 0) return -1;
    ssize_t n = send(m_hSocket, buf, (size_t)len, flags);
    if (n < 0) SetLastError((DWORD)errno);
    return (int)n;
}

int CAsyncSocket::Receive(void* buf, int len, int flags) {
    if (m_hSocket < 0 || !buf || len <= 0) return -1;
    ssize_t n = recv(m_hSocket, buf, (size_t)len, flags);
    if (n < 0) SetLastError((DWORD)errno);
    return (int)n;
}

void CAsyncSocket::Close() {
    Unregister();
    if (m_hSocket >= 0) {
        close(m_hSocket);
        m_hSocket = -1;
    }
    m_connecting = 0;
}

BOOL CAsyncSocket::GetSockName(SOCKADDR* p, int* len) {
    if (m_hSocket < 0 || !p || !len) return FALSE;
    socklen_t l = (socklen_t)*len;
    if (getsockname(m_hSocket, (struct sockaddr*)p, &l) != 0) return FALSE;
    *len = (int)l;
    return TRUE;
}

BOOL CAsyncSocket::GetPeerName(SOCKADDR* p, int* len) {
    if (m_hSocket < 0 || !p || !len) return FALSE;
    socklen_t l = (socklen_t)*len;
    if (getpeername(m_hSocket, (struct sockaddr*)p, &l) != 0) return FALSE;
    *len = (int)l;
    return TRUE;
}

BOOL CAsyncSocket::AsyncSelect(long lEvent) {
    Register(lEvent);
    return TRUE;
}

BOOL CAsyncSocket::IOCtl(long lCommand, DWORD* lpArgument) {
    if (m_hSocket < 0) return FALSE;
    // FIONBIO is the only one the engine uses, and the socket is already non-blocking.
    if (lpArgument) *lpArgument = 0;
    (void)lCommand;
    return TRUE;
}

BOOL CAsyncSocket::ShutDown(int nHow) {
    if (m_hSocket < 0) return FALSE;
    return shutdown(m_hSocket, nHow) == 0;
}

BOOL CAsyncSocket::Attach(int hSocket, long lEvent) {
    Close();
    m_hSocket = hSocket;
    SetNonBlocking(m_hSocket);
    Register(lEvent);
    return TRUE;
}

int CAsyncSocket::Detach() {
    Unregister();
    int s = m_hSocket;
    m_hSocket = -1;
    return s;
}

// The run loop's callback. Dispatches to the MFC virtuals, which is where the engine's own
// IRC state machine lives (CIrcSocket::OnReceive parses the protocol line by line).
static void NativeAsyncSocketCallback(CFSocketRef s, CFSocketCallBackType type,
                                      CFDataRef /*addr*/, const void* data, void* info) {
    CAsyncSocket* self = (CAsyncSocket*)info;
    if (!self || self->m_hSocket < 0) return;
    if (Trace())
        fprintf(stderr, "[sock %d] callback type=%lu data=%p\n",
                self->m_hSocket, (unsigned long)type, data);

    if (type == kCFSocketConnectCallBack) {
        // data is NULL on success, or a pointer to the error on failure.
        int err = 0;
        if (data) {
            err = *(const int*)data;
        } else {
            // Confirm with SO_ERROR: the callback firing is not by itself proof the connect
            // succeeded.
            int so = 0;
            socklen_t l = sizeof(so);
            if (getsockopt(self->m_hSocket, SOL_SOCKET, SO_ERROR, &so, &l) == 0) err = so;
        }
        self->m_connecting = 0;
        self->OnConnect(err);
        if (err == 0) {
            // Clear FD_CONNECT before re-arming. Re-registering WITH it recreates a CFSocket
            // that has a connect callback on an already-connected socket, which CFSocket
            // delivers immediately - so OnConnect fired in a loop and the engine kept
            // re-sending its registration instead of moving on. Reads and close are what
            // matter from here.
            self->m_events &= ~FD_CONNECT;
            self->Register(self->m_events);
        }
        return;
    }

    if (type == kCFSocketReadCallBack) {
        // Distinguish "data available" from "peer closed" without consuming anything: MSG_PEEK
        // returning 0 is EOF. Calling OnReceive on EOF instead would leave the engine looping
        // on a socket that will never produce another byte.
        char probe;
        ssize_t n = recv(self->m_hSocket, &probe, 1, MSG_PEEK);
        if (n == 0) {
            self->OnClose(0);
            return;
        }
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            self->OnClose(errno);
            return;
        }
        self->OnReceive(0);
        // CFSocket read callbacks are one-shot per source unless re-enabled.
        if (self->m_cfSocket)
            CFSocketEnableCallBacks((CFSocketRef)self->m_cfSocket, kCFSocketReadCallBack);
        return;
    }

    if (type == kCFSocketAcceptCallBack) {
        self->OnAccept(0);
        return;
    }
    (void)s;
}
