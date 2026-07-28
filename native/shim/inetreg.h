// inetreg.h - Internet Explorer's registry-key-name header, included by intl.c.
//
// On Windows this comes from the IE SDK and is nothing but string constants naming
// registry paths and value names (the international/encoding preferences). intl.c reads
// the user's preferred encoding through them.
//
// Near-empty deliberately, on the same principle as afxpriv.h: whatever intl.c actually
// references shows up as a named compile error, which beats guessing at the contents of a
// header we do not have. The native build has no such registry to read, so the values
// only need to exist, not to be correct - and the registry accessors in winreg.h already
// report failure, so intl.c takes its default-encoding path either way.

#ifndef NATIVE_SHIM_INETREG_H
#define NATIVE_SHIM_INETREG_H

#endif // NATIVE_SHIM_INETREG_H
