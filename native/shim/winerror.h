// winerror.h - the HRESULT/Win32 error constants. ratings.h includes it.
// The values are the real ones; several are already defined in win32types.h and
// gdishim.h, so this only adds what is missing and guards each.
#ifndef NATIVE_SHIM_WINERROR_H
#define NATIVE_SHIM_WINERROR_H
#include "win32types.h"
#ifndef E_UNEXPECTED
#define E_UNEXPECTED    ((HRESULT)0x8000FFFFL)
#endif
#ifndef E_NOINTERFACE
#define E_NOINTERFACE   ((HRESULT)0x80004002L)
#endif
#ifndef E_POINTER
#define E_POINTER       ((HRESULT)0x80004003L)
#endif
#ifndef E_ABORT
#define E_ABORT         ((HRESULT)0x80004004L)
#endif
#ifndef E_ACCESSDENIED
#define E_ACCESSDENIED  ((HRESULT)0x80070005L)
#endif
#ifndef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE 6L
#endif
#ifndef ERROR_MORE_DATA
#define ERROR_MORE_DATA 234L
#endif
#ifndef ERROR_NO_MORE_ITEMS
#define ERROR_NO_MORE_ITEMS 259L
#endif
#define MAKE_HRESULT(sev, fac, code) \
    ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))
#define SEVERITY_SUCCESS 0
#define SEVERITY_ERROR   1
#define FACILITY_ITF     4
#endif
