// winreg.h - registry API. chatdoc.cpp reads user preferences from it.
//
// Every call fails. That is deliberate rather than lazy: the engine treats a failed
// registry read as "no stored preference" and falls back to its defaults, which is
// exactly right for a first native run. Persisting settings on macOS should use
// NSUserDefaults behind a small interface, not an emulated registry.
#ifndef NATIVE_SHIM_WINREG_H
#define NATIVE_SHIM_WINREG_H
#include "win32types.h"

#define HKEY_CLASSES_ROOT   ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER   ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE  ((HKEY)(uintptr_t)0x80000002)
#define KEY_READ            0x20019
#define KEY_WRITE           0x20006
#define KEY_ALL_ACCESS      0xF003F
#define REG_SZ              1
#define REG_BINARY          3
#define REG_DWORD           4
#define REG_OPTION_NON_VOLATILE 0

inline LONG RegOpenKeyEx(HKEY, LPCSTR, DWORD, DWORD, HKEY*) { return ERROR_FILE_NOT_FOUND; }
inline LONG RegOpenKey(HKEY, LPCSTR, HKEY*) { return ERROR_FILE_NOT_FOUND; }
inline LONG RegCreateKeyEx(HKEY, LPCSTR, DWORD, LPSTR, DWORD, DWORD, void*, HKEY*, LPDWORD) { return ERROR_FILE_NOT_FOUND; }
inline LONG RegQueryValueEx(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) { return ERROR_FILE_NOT_FOUND; }
inline LONG RegQueryValue(HKEY, LPCSTR, LPSTR, LONG*) { return ERROR_FILE_NOT_FOUND; }
inline LONG RegSetValueEx(HKEY, LPCSTR, DWORD, DWORD, const BYTE*, DWORD) { return ERROR_FILE_NOT_FOUND; }
inline LONG RegDeleteKey(HKEY, LPCSTR) { return ERROR_FILE_NOT_FOUND; }
inline LONG RegDeleteValue(HKEY, LPCSTR) { return ERROR_FILE_NOT_FOUND; }
inline LONG RegCloseKey(HKEY) { return ERROR_SUCCESS; }
inline LONG RegEnumKeyEx(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, void*) { return ERROR_FILE_NOT_FOUND; }

#endif
