// ocidl.h stand-in for the native build. The MIDL-generated icchat.h pulls the COM
// interface headers in; ActiveX automation is dropped here, so these resolve the
// includes without declaring real interfaces.
#ifndef NATIVE_SHIM_OCIDL_H
#define NATIVE_SHIM_OCIDL_H
#include "win32types.h"
#endif
