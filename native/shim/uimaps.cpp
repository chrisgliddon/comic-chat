// uimaps.cpp - a HOLDING PEN, not a design. Delete entries as their real .cpp joins the
// link.
//
// Making the message map real (msgmap.h) turned DECLARE_MESSAGE_MAP into three actual
// members, one of them virtual. A virtual lands in the vtable, and a vtable is emitted for
// any class that gets instantiated - so every class that declares a map and IS constructed
// now needs its map DEFINED, even if its own .cpp is not linked yet. Previously the macro
// expanded to nothing and the question never arose.
//
// Three classes are in that position. Each is instantiated by code that is linked, while
// its own source is not:
//
//   CChatApp   (chat.h)     - theApp itself, constructed in nativeapp.cpp.
//                             chat.cpp needs: OLE_APPTYPE, COleResizeBar, COleDropTarget,
//                             direct.h.
//   CRtfCtrl   (rtfctrl.h)  - the say window's base, reached through nativeapp.cpp.
//                             rtfctrl.cpp needs: CToolBarCtrl::CheckButton and the rest of
//                             the toolbar control surface.
//   CTextView  (textview.h) - CStatusView's base, so status.cpp's map chains to it.
//                             textview.cpp needs: EN_LINK/ENM_LINK, CRichEditCtrl::
//                             SetEventMask.
//
// An empty map here means "messages fall through to the base class", which for these three
// is the truthful answer right now: their handlers are not linked, so there is nothing to
// dispatch to. It is NOT a claim that they have no handlers. When a file below starts
// compiling, its own BEGIN_MESSAGE_MAP takes over and the entry here becomes a duplicate
// symbol - which is the intended way to be told to delete it.

#include "stdafx.h"
#include "chat.h"
#include "rtfctrl.h"
#include "textview.h"

BEGIN_MESSAGE_MAP(CChatApp, CWinApp)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CRtfCtrl, CRichEditCtrl)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CTextView, CView)
END_MESSAGE_MAP()

// Declared in textview.h and named by CStatusView's map, defined in textview.cpp. Stubbed
// so status.o links; the real one shows the text-view context menu.
void CTextView::OnContextMenu(CWnd*, CPoint) {}
