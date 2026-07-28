// sspi.h - the sliver of the Windows SSPI surface that csspi.h does not carry.
//
// artifacts/inc/csspi.h is Comic Chat's own cut-down SSPI header (SecurityFunctionTable,
// SECPKG_CRED_OUTBOUND, SEC_E_*), and ircsock.h includes it. What it leaves to the real
// Windows sspi.h is the credential struct, which HrAuthenticate fills in.
//
// SCOPE, stated plainly: this makes ircsock.cpp COMPILE. It cannot make SSPI WORK.
// The authentication path calls LoadLibrary("security.dll") and dispatches through
// INIT_SECURITY_INTERFACE - a Windows NTLM/Negotiate implementation with no macOS
// counterpart. HrAuthenticate will fail at the LoadLibrary step and return E_FAIL.
//
// That is an acceptable gap rather than a parity hole worth closing, because the servers
// this path existed for are gone: SSPI auth here targeted Microsoft Chat Server, which
// used NTLM over IRCX. Real IRC networks authenticate with PASS, SASL PLAIN/EXTERNAL or
// NickServ, none of which come through here. Recorded in native/README.md as a known
// non-parity item so it is a decision on the record and not an oversight.

#ifndef NATIVE_SHIM_SSPI_H
#define NATIVE_SHIM_SSPI_H

#include "win32types.h"

// Flags telling SSPI how the strings in the struct below are encoded. The engine only
// ever sets _ANSI.
#define SEC_WINNT_AUTH_IDENTITY_ANSI     0x1
#define SEC_WINNT_AUTH_IDENTITY_UNICODE  0x2

// Layout matches Windows' SEC_WINNT_AUTH_IDENTITY_A. Nothing off-Windows reads it, but
// keeping the real field order and types means the assignments in HrAuthenticate compile
// unchanged and stay reviewable against the Windows original.
typedef struct _SEC_WINNT_AUTH_IDENTITY_A {
    unsigned char*  User;
    unsigned long   UserLength;
    unsigned char*  Domain;
    unsigned long   DomainLength;
    unsigned char*  Password;
    unsigned long   PasswordLength;
    unsigned long   Flags;
} SEC_WINNT_AUTH_IDENTITY_A, *PSEC_WINNT_AUTH_IDENTITY_A;

typedef SEC_WINNT_AUTH_IDENTITY_A   SEC_WINNT_AUTH_IDENTITY;
typedef PSEC_WINNT_AUTH_IDENTITY_A  PSEC_WINNT_AUTH_IDENTITY;

#endif // NATIVE_SHIM_SSPI_H
