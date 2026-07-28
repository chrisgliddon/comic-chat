// rpcndr.h stand-in. icchat.h (MIDL-generated) tests
// __REQUIRED_RPCNDR_H_VERSION__ >= 475 and #errors out below that, so the version
// must be declared even though no NDR marshalling exists here.
#ifndef NATIVE_SHIM_RPCNDR_H
#define NATIVE_SHIM_RPCNDR_H
#include "win32types.h"
// icchat.h errors out unless __RPCNDR_H_VERSION__ is defined at all; the value
// only has to clear its __REQUIRED_RPCNDR_H_VERSION__ of 475.
#define __RPCNDR_H_VERSION__ 500
typedef GUID UUID;
typedef long RPC_STATUS;
typedef BYTE boolean;

// COM's `interface` keyword is a macro for `struct` in MSVC's headers; the
// MIDL-generated icchat.h declares its interfaces with it. Also the DECLSPEC and
// calling-convention decorations that appear on those declarations.
#ifndef interface
#define interface struct
#endif
#define DECLSPEC_UUID(x)
#define MIDL_INTERFACE(x)   struct
#define STDMETHODCALLTYPE
#define STDMETHOD(m)        virtual HRESULT m
#define STDMETHOD_(t, m)    virtual t m
#define PURE                = 0
#define DECLARE_INTERFACE(i)        interface i
#define DECLARE_INTERFACE_(i, b)    interface i : public b
#define EXTERN_C            extern "C"
#define STDAPI              extern "C" HRESULT
#define STDAPI_(t)          extern "C" t
#define RPC_S_OK 0
#endif
