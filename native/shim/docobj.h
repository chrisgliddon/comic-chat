// docobj.h - stand-in for the OLE DocObject interface header.
//
// binddoc.h includes it for the IOleCommandTarget/OLECMD declarations. ActiveX
// embedding is dropped from the native build entirely, so these are names only.

#ifndef NATIVE_SHIM_DOCOBJ_H
#define NATIVE_SHIM_DOCOBJ_H

#include "win32types.h"

typedef struct _tagOLECMD { ULONG cmdID; DWORD cmdf; } OLECMD, *POLECMD;
typedef struct _tagOLECMDTEXT { DWORD cmdtextf; ULONG cwActual, cwBuf; WCHAR rgwz[1]; } OLECMDTEXT;
typedef void* IOleCommandTarget;
typedef void* IOleDocumentView;
typedef void* IOleDocument;

#define OLECMDF_SUPPORTED   0x00000001
#define OLECMDF_ENABLED     0x00000002
#define OLECMDF_LATCHED     0x00000004
#define OLECMDF_NINCHED     0x00000008

#endif // NATIVE_SHIM_DOCOBJ_H
