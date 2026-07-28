// afxpriv.h - MFC's internal header, included by chatsrv.cpp.
//
// Deliberately near-empty. Whatever chatsrv.cpp actually needs from MFC internals shows
// up as a compile error naming the symbol, which is a better way to discover the real
// dependency than guessing at the contents of a private header. Anything genuinely
// required gets added to the header it belongs in (mfcshim.h / mfcui.h) rather than here,
// so this file stays a placeholder that satisfies the #include and nothing more.

#ifndef NATIVE_SHIM_AFXPRIV_H
#define NATIVE_SHIM_AFXPRIV_H

#include "mfcshim.h"
#include "mfcui.h"

#endif // NATIVE_SHIM_AFXPRIV_H
