// richedit.h stand-in. format.cpp includes it for the CHARFORMAT/PARAFORMAT
// definitions used when building RTF for the text view. CHARFORMAT itself lives in
// gdishim.h (chat.h needs it too); this adds the rest.
#ifndef NATIVE_SHIM_RICHEDIT_H
#define NATIVE_SHIM_RICHEDIT_H
#include "win32types.h"
#include "gdishim.h"

#define PFM_STARTINDENT     0x00000001
#define PFM_RIGHTINDENT     0x00000002
#define PFM_OFFSET          0x00000004
#define PFM_ALIGNMENT       0x00000008
#define PFA_LEFT            1
#define PFA_RIGHT           2
#define PFA_CENTER          3

typedef struct _paraformat {
    UINT  cbSize;
    DWORD dwMask;
    WORD  wNumbering, wReserved;
    LONG  dxStartIndent, dxRightIndent, dxOffset;
    WORD  wAlignment;
    SHORT cTabCount;
    LONG  rgxTabs[32];
} PARAFORMAT;

typedef struct _charrange { LONG cpMin, cpMax; } CHARRANGE;
typedef struct _textrange { CHARRANGE chrg; LPSTR lpstrText; } TEXTRANGE;

#define EM_SETCHARFORMAT    (WM_USER + 68)
#define EM_GETCHARFORMAT    (WM_USER + 58)
#define EM_SETPARAFORMAT    (WM_USER + 71)
#define EM_STREAMIN         (WM_USER + 73)
#define EM_STREAMOUT        (WM_USER + 74)
#define SCF_SELECTION       0x0001
#define SF_RTF              0x0002
#define SF_TEXT             0x0001

#endif
