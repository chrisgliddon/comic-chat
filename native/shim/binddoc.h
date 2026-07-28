// binddoc.h - SHADOWS the engine's binddoc.h for the native build.
//
// This is the first shim header that replaces an engine header rather than
// supplying a missing platform one, so the reasoning matters.
//
// The engine's binddoc.h declares one class, CDocObjectServerDoc, and implements
// it as a full OLE DocObject server: nested BEGIN_INTERFACE_PART blocks for
// IOleObject, IOleDocument, IOleInPlaceObject, IDataObject, IPersistStorage and
// friends. That machinery exists so Comic Chat can be embedded as an ActiveX
// control inside Internet Explorer. The native macOS app is not embeddable and
// never will be, so none of it is being ported.
//
// The problem is that CChatDoc derives from CDocObjectServerDoc (chatdoc.h), and
// 44 engine .cpp files include binddoc.h transitively. So the base class has to
// exist even though the native build wants none of its behaviour. The options
// were:
//
//   1. Shim the ~30 COM interfaces the real header names. Large, and every one of
//      them would be a stub that looks like OLE support while being none.
//   2. Shadow the header with an empty base class. Chosen.
//
// stage.sh links engine headers first and shim headers second with `ln -sf`, so
// this file wins the name.
//
// The cost is explicit: any engine code that actually calls a DocObject method
// will not compile against this, which is the correct outcome - it flags code
// that belongs to the embedding story rather than the app.

#ifndef NATIVE_SHIM_BINDDOC_H
#define NATIVE_SHIM_BINDDOC_H

#include "win32types.h"
#include "mfcshim.h"
#include "mfcui.h"

// Empty stand-in for the OLE DocObject server document. CChatDoc derives from
// this; on macOS it contributes nothing but the base.
class CDocObjectServerDoc : public COleServerDoc {
public:
    // m_pOrigParent is a member of the real class that chatdoc.h reads directly.
    // Kept (as null) rather than removed, because the reference is in engine code
    // that otherwise compiles.
    CWnd* m_pOrigParent;

    CDocObjectServerDoc() : m_pOrigParent(0) {}
    virtual ~CDocObjectServerDoc() {}
    virtual BOOL SaveModified() { return TRUE; }
};

#endif // NATIVE_SHIM_BINDDOC_H
