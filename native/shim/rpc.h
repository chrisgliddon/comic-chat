// rpc.h / rpcndr.h stand-in. Pulled in by the OLE headers for UUID and
// marshalling types; the native build does no RPC.
#ifndef NATIVE_SHIM_RPC_H
#define NATIVE_SHIM_RPC_H
#include "win32types.h"
typedef GUID UUID;
typedef long RPC_STATUS;
#define RPC_S_OK 0
#endif
