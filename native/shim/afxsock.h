// afxsock.h - MFC's socket header. The real one declares CAsyncSocket and CSocket;
// both are in mfcui.h here, and the BSD mapping is in winsock.h.
//
// ircproto.cpp includes this directly (not via stdafx.h), which is why it exists as a
// file rather than being folded into the shim's stdafx.
#ifndef NATIVE_SHIM_AFXSOCK_H
#define NATIVE_SHIM_AFXSOCK_H
#include "win32types.h"
#include "winsock.h"
#include "mfcshim.h"
#include "mfcui.h"

// CSocket is the blocking subclass of CAsyncSocket. Declared for completeness; the
// engine's own chatsock replaces most of it.
class CSocket : public CAsyncSocket {
public:
    BOOL Create(UINT = 0, int = 0, LPCTSTR = 0) { return FALSE; }
    virtual BOOL Connect(LPCTSTR, UINT) { return FALSE; }
};
#endif
