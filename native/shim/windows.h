// windows.h - satisfies `#include <windows.h>` from the artifacts/inc headers
// (ccomp.h reaches for it) by redirecting to the shim's type floor.
//
// Named windows.h on purpose: the alternative was editing artifacts/inc, which is
// vendored reference material shared with the Windows build.

#ifndef NATIVE_SHIM_WINDOWS_H
#define NATIVE_SHIM_WINDOWS_H

#include "win32types.h"

#endif // NATIVE_SHIM_WINDOWS_H
