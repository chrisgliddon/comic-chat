// mmsystem.h - the multimedia API surface the engine names. Sound playback is not
// part of the native port (mcithrd.cpp is dropped); these exist so protsupp.cpp and
// the notification code parse.
//
// If sound is ever wanted, the answer is AVFoundation behind a small interface -
// not filling these in, since the MCI command model has no macOS counterpart.
#ifndef NATIVE_SHIM_MMSYSTEM_H
#define NATIVE_SHIM_MMSYSTEM_H
#include "win32types.h"

#define SND_SYNC        0x0000
#define SND_ASYNC       0x0001
#define SND_NODEFAULT   0x0002
#define SND_MEMORY      0x0004
#define SND_LOOP        0x0008
#define SND_NOSTOP      0x0010
#define SND_FILENAME    0x00020000
#define SND_RESOURCE    0x00040004

typedef UINT MCIDEVICEID;
typedef DWORD MCIERROR;

inline BOOL PlaySound(LPCSTR, HINSTANCE, DWORD) { return FALSE; }
inline BOOL sndPlaySound(LPCSTR, UINT) { return FALSE; }
inline MCIERROR mciSendString(LPCSTR, LPSTR ret, UINT n, HWND) { if (ret && n) ret[0] = 0; return 1; }
inline MCIERROR mciSendCommand(MCIDEVICEID, UINT, DWORD, DWORD_PTR) { return 1; }
inline UINT mciGetErrorString(MCIERROR, LPSTR buf, UINT n) { if (buf && n) buf[0] = 0; return 0; }
inline DWORD timeGetTime() { return 0; }

#endif
