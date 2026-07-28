// imm.h - the Input Method Manager API. pageview.cpp uses it for IME composition
// positioning when typing CJK text.
//
// Stubs: macOS text input goes through NSTextInputClient, which has no relationship
// to IMM's HIMC model. A native app wanting CJK input implements that protocol on
// the view instead. Related open question: Tier-1 #13 (the jis2sjis/intl path).
#ifndef NATIVE_SHIM_IMM_H
#define NATIVE_SHIM_IMM_H
#include "win32types.h"
typedef void* HIMC;
typedef struct tagCOMPOSITIONFORM { DWORD dwStyle; POINT ptCurrentPos; RECT rcArea; } COMPOSITIONFORM;
#define CFS_DEFAULT     0x0000
#define CFS_RECT        0x0001
#define CFS_POINT       0x0002
#define CFS_FORCE_POSITION 0x0020
inline HIMC ImmGetContext(HWND) { return (HIMC)0; }
inline BOOL ImmReleaseContext(HWND, HIMC) { return TRUE; }
inline BOOL ImmSetCompositionWindow(HIMC, COMPOSITIONFORM*) { return FALSE; }
inline BOOL ImmSetCompositionFont(HIMC, void*) { return FALSE; }
inline LONG ImmGetCompositionString(HIMC, DWORD, void*, DWORD) { return 0; }
inline BOOL ImmGetOpenStatus(HIMC) { return FALSE; }
#endif
