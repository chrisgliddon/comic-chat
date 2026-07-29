// msgmap.h - a REAL MFC message map, replacing the macros that compiled it away.
//
// WHY THIS FILE EXISTS. Everything the original UI does is already in the tree and
// already compiles: CBodyCam::OnPaint draws the self-view and the emotion wheel,
// CChatDoc's map holds the whole command surface (ID_ACTIONS_SAY, ID_VIEW_COMICS,
// ID_MEMBER_GETINFO, the formatting switches, the ID_MACRO_A0..A9 range), CPageView
// handles the mouse. None of it could ever run, because the shim expanded every
// BEGIN_MESSAGE_MAP body to nothing. There was no table and no dispatcher, so a click or
// a menu pick had nowhere to go - and the only way to get anything on screen was to write
// fresh UI code duplicating what these files already say. That was the wrong direction.
//
// So: build the table. One dispatcher makes every one of those handlers reachable at once,
// and the host's job shrinks to delivering events rather than deciding what they mean.
//
// HOW IT MATCHES MFC. The structure is MFC's, because the macros in the engine sources are
// MFC's and cannot be changed:
//
//   - handlers are NOT virtual. A derived class's OnPaint hides the base's rather than
//     overriding it, so only the table knows which one to call. Hence the stored
//     pointer-to-member.
//   - the table lives inside the body of GetThisMessageMap(), a member function of the
//     class. That is what makes `&ThisClass::OnPaint` legal for a PROTECTED handler, which
//     nearly every afx_msg handler in the tree is. It is also where `ThisClass` comes from:
//     a function-local typedef, so the entry macros need not name the class.
//   - each map chains to its base's, so an unhandled message walks up the hierarchy.
//
// The one unavoidable liberty is MFC's own: a handler's real signature is recovered by
// casting the stored pointer-to-member back before the call. That is sound only because
// every class dispatched here is single-inheritance with no virtual bases (CBodyCam ->
// CWnd -> CCmdTarget -> CObject). A class with multiple inheritance must not be added to a
// map without revisiting AFX_PMSG_CAST.
//
// Include position: after the basic Win32 typedefs (UINT, LRESULT, HMENU), before any
// engine source. mfcui.h includes it at the point where the old dead macro block sat.

#ifndef NATIVE_MSGMAP_H
#define NATIVE_MSGMAP_H

class CCmdTarget;

// The signature tag. Every distinct handler shape used by a map in this build needs one,
// because the dispatcher has to unpack WPARAM/LPARAM before it can call.
// Naming follows MFC's: return type first, then arguments.
//   v void   b BOOL   i int   w UINT   l LRESULT/LPARAM   s short
//   p CWnd*  P CPoint   D CDC*   U CCmdUI*   h HMENU   C LPCREATESTRUCT   x CScrollBar*
//   N NMHDR*
enum AfxSig {
    AfxSig_end = 0,     // terminator
    AfxSig_vv,          // void ()                        OnPaint, OnDestroy, ON_COMMAND
    AfxSig_vw,          // void (UINT)                    OnTimer, ON_COMMAND_RANGE
    AfxSig_vwP,         // void (UINT, CPoint)            mouse buttons, OnMouseMove
    AfxSig_vwww,        // void (UINT, UINT, UINT)        OnKeyDown, OnChar
    AfxSig_vwii,        // void (UINT, int, int)          OnSize
    AfxSig_vp,          // void (CWnd*)                   OnSetFocus, OnPaletteChanged
    AfxSig_vpP,         // void (CWnd*, CPoint)           OnContextMenu
    AfxSig_vwp,         // void (UINT, CWnd*)             OnEnterIdle
    AfxSig_vwwh,        // void (UINT, UINT, HMENU)       OnMenuSelect
    AfxSig_vwwx,        // void (UINT, UINT, CScrollBar*) OnVScroll, OnHScroll
    AfxSig_vbw,         // void (BOOL, UINT)              OnShowWindow
    AfxSig_wv,          // UINT ()                        OnGetDlgCode
    AfxSig_iC,          // int  (LPCREATESTRUCT)          OnCreate
    AfxSig_bD,          // BOOL (CDC*)                    OnEraseBkgnd
    AfxSig_bwsP,        // BOOL (UINT, short, CPoint)     OnMouseWheel
    AfxSig_lwl,         // LRESULT (WPARAM, LPARAM)       ON_MESSAGE
    AfxSig_vU,          // void (CCmdUI*)                 ON_UPDATE_COMMAND_UI
    AfxSig_vNl,         // void (NMHDR*, LRESULT*)        ON_NOTIFY
    // A macro that appears in the sources but whose shape is not dispatched yet. It still
    // occupies a well-formed slot and the dispatcher skips it. Better than expanding to
    // nothing, which is how the whole surface went dark in the first place.
    AfxSig_unsupported
};

// A pointer to SOME member of SOME CCmdTarget-derived class, signature erased.
// The entry's nSig says how to put it back.
typedef void (CCmdTarget::*AFX_PMSG)(void);

struct AFX_MSGMAP_ENTRY {
    UINT     nMessage;      // WM_*, or 0 for a command / update-UI entry
    UINT     nCode;         // notification code, or CN_COMMAND / CN_UPDATE_COMMAND_UI
    UINT     nID;           // command id, or the first id of a range
    UINT     nLastID;       // last id of a range; == nID when not a range
    UINT     nSig;          // AfxSig
    AFX_PMSG pfn;
};

struct AFX_MSGMAP {
    const AFX_MSGMAP* (*pfnGetBaseMap)();   // NULL ends the chain (CCmdTarget)
    const AFX_MSGMAP_ENTRY* lpEntries;
};

// Command routing codes, matching MFC's so nothing collides with a real WM_.
#define CN_COMMAND           0
#define CN_UPDATE_COMMAND_UI ((UINT)(-1))

// The one questionable cast in the design, named so it has a single place to be reasoned
// about. See the header comment on single inheritance.
#define AFX_PMSG_CAST(fn) ((AFX_PMSG)(fn))

// --- map declaration and definition ----------------------------------------

#define DECLARE_MESSAGE_MAP() \
protected: \
    static const AFX_MSGMAP* GetThisMessageMap(); \
    virtual const AFX_MSGMAP* GetMessageMap() const;

// Everything happens inside GetThisMessageMap's body: the ThisClass typedef the entry
// macros use, the table as a function-local static, and the map that chains to the base.
#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
    const AFX_MSGMAP* theClass::GetMessageMap() const { return GetThisMessageMap(); } \
    const AFX_MSGMAP* theClass::GetThisMessageMap() { \
        typedef theClass ThisClass; \
        typedef baseClass TheBaseClass; \
        static const AFX_MSGMAP_ENTRY _messageEntries[] = {

#define END_MESSAGE_MAP() \
            { 0, 0, 0, 0, AfxSig_end, (AFX_PMSG)0 } \
        }; \
        static const AFX_MSGMAP _messageMap = \
            { &TheBaseClass::GetThisMessageMap, &_messageEntries[0] }; \
        return &_messageMap; \
    }

// --- command and notification entries --------------------------------------

#define ON_COMMAND(id, fn) \
    { 0, CN_COMMAND, (UINT)(id), (UINT)(id), AfxSig_vv, AFX_PMSG_CAST(&ThisClass::fn) },

// A range handler receives the id that fired, so it is void (UINT).
#define ON_COMMAND_RANGE(a, b, fn) \
    { 0, CN_COMMAND, (UINT)(a), (UINT)(b), AfxSig_vw, AFX_PMSG_CAST(&ThisClass::fn) },

#define ON_UPDATE_COMMAND_UI(id, fn) \
    { 0, CN_UPDATE_COMMAND_UI, (UINT)(id), (UINT)(id), AfxSig_vU, \
      AFX_PMSG_CAST(&ThisClass::fn) },

#define ON_UPDATE_COMMAND_UI_RANGE(a, b, fn) \
    { 0, CN_UPDATE_COMMAND_UI, (UINT)(a), (UINT)(b), AfxSig_vU, \
      AFX_PMSG_CAST(&ThisClass::fn) },

#define ON_MESSAGE(msg, fn) \
    { (UINT)(msg), 0, 0, 0, AfxSig_lwl, AFX_PMSG_CAST(&ThisClass::fn) },

#define ON_REGISTERED_MESSAGE(msg, fn) \
    { 0xC000, (UINT)(msg), 0, 0, AfxSig_lwl, AFX_PMSG_CAST(&ThisClass::fn) },

#define ON_NOTIFY(code, id, fn) \
    { WM_NOTIFY, (UINT)(int)(code), (UINT)(id), (UINT)(id), AfxSig_vNl, \
      AFX_PMSG_CAST(&ThisClass::fn) },

#define ON_NOTIFY_RANGE(code, a, b, fn) \
    { WM_NOTIFY, (UINT)(int)(code), (UINT)(a), (UINT)(b), AfxSig_vNl, \
      AFX_PMSG_CAST(&ThisClass::fn) },

// Control notifications route like commands: WM_COMMAND with a notification code.
#define ON_CONTROL(code, id, fn) \
    { WM_COMMAND, (UINT)(code), (UINT)(id), (UINT)(id), AfxSig_vv, \
      AFX_PMSG_CAST(&ThisClass::fn) },

#define ON_EN_CHANGE(id, fn)        ON_CONTROL(EN_CHANGE, id, fn)
#define ON_EN_SETFOCUS(id, fn)      ON_CONTROL(EN_SETFOCUS, id, fn)
#define ON_EN_KILLFOCUS(id, fn)     ON_CONTROL(EN_KILLFOCUS, id, fn)
#define ON_BN_CLICKED(id, fn)       ON_CONTROL(BN_CLICKED, id, fn)
#define ON_BN_DOUBLECLICKED(id, fn) ON_CONTROL(BN_DOUBLECLICKED, id, fn)
#define ON_CBN_SELCHANGE(id, fn)    ON_CONTROL(CBN_SELCHANGE, id, fn)
#define ON_CBN_EDITCHANGE(id, fn)   ON_CONTROL(CBN_EDITCHANGE, id, fn)
#define ON_CBN_DROPDOWN(id, fn)     ON_CONTROL(CBN_DROPDOWN, id, fn)
#define ON_CBN_KILLFOCUS(id, fn)    ON_CONTROL(CBN_KILLFOCUS, id, fn)
#define ON_CBN_SETFOCUS(id, fn)     ON_CONTROL(CBN_SETFOCUS, id, fn)
#define ON_CBN_CLOSEUP(id, fn)      ON_CONTROL(CBN_CLOSEUP, id, fn)
#define ON_LBN_SELCHANGE(id, fn)    ON_CONTROL(LBN_SELCHANGE, id, fn)
#define ON_LBN_DBLCLK(id, fn)       ON_CONTROL(LBN_DBLCLK, id, fn)

// --- window message entries -------------------------------------------------
// Each macro knows its handler's name, exactly as MFC's does: ON_WM_PAINT() implies
// OnPaint. That is why they cannot collapse into one variadic macro.

#define ON_WM_CREATE() \
    { WM_CREATE, 0, 0, 0, AfxSig_iC, AFX_PMSG_CAST(&ThisClass::OnCreate) },
#define ON_WM_DESTROY() \
    { WM_DESTROY, 0, 0, 0, AfxSig_vv, AFX_PMSG_CAST(&ThisClass::OnDestroy) },
#define ON_WM_NCDESTROY() \
    { WM_NCDESTROY, 0, 0, 0, AfxSig_vv, AFX_PMSG_CAST(&ThisClass::OnNcDestroy) },
#define ON_WM_PAINT() \
    { WM_PAINT, 0, 0, 0, AfxSig_vv, AFX_PMSG_CAST(&ThisClass::OnPaint) },
#define ON_WM_SIZE() \
    { WM_SIZE, 0, 0, 0, AfxSig_vwii, AFX_PMSG_CAST(&ThisClass::OnSize) },
#define ON_WM_TIMER() \
    { WM_TIMER, 0, 0, 0, AfxSig_vw, AFX_PMSG_CAST(&ThisClass::OnTimer) },
#define ON_WM_LBUTTONDOWN() \
    { WM_LBUTTONDOWN, 0, 0, 0, AfxSig_vwP, AFX_PMSG_CAST(&ThisClass::OnLButtonDown) },
#define ON_WM_LBUTTONUP() \
    { WM_LBUTTONUP, 0, 0, 0, AfxSig_vwP, AFX_PMSG_CAST(&ThisClass::OnLButtonUp) },
#define ON_WM_LBUTTONDBLCLK() \
    { WM_LBUTTONDBLCLK, 0, 0, 0, AfxSig_vwP, AFX_PMSG_CAST(&ThisClass::OnLButtonDblClk) },
#define ON_WM_RBUTTONDOWN() \
    { WM_RBUTTONDOWN, 0, 0, 0, AfxSig_vwP, AFX_PMSG_CAST(&ThisClass::OnRButtonDown) },
#define ON_WM_RBUTTONUP() \
    { WM_RBUTTONUP, 0, 0, 0, AfxSig_vwP, AFX_PMSG_CAST(&ThisClass::OnRButtonUp) },
#define ON_WM_MOUSEMOVE() \
    { WM_MOUSEMOVE, 0, 0, 0, AfxSig_vwP, AFX_PMSG_CAST(&ThisClass::OnMouseMove) },
#define ON_WM_KEYDOWN() \
    { WM_KEYDOWN, 0, 0, 0, AfxSig_vwww, AFX_PMSG_CAST(&ThisClass::OnKeyDown) },
#define ON_WM_KEYUP() \
    { WM_KEYUP, 0, 0, 0, AfxSig_vwww, AFX_PMSG_CAST(&ThisClass::OnKeyUp) },
#define ON_WM_CHAR() \
    { WM_CHAR, 0, 0, 0, AfxSig_vwww, AFX_PMSG_CAST(&ThisClass::OnChar) },
#define ON_WM_SETFOCUS() \
    { WM_SETFOCUS, 0, 0, 0, AfxSig_vp, AFX_PMSG_CAST(&ThisClass::OnSetFocus) },
#define ON_WM_KILLFOCUS() \
    { WM_KILLFOCUS, 0, 0, 0, AfxSig_vp, AFX_PMSG_CAST(&ThisClass::OnKillFocus) },
#define ON_WM_PALETTECHANGED() \
    { WM_PALETTECHANGED, 0, 0, 0, AfxSig_vp, \
      AFX_PMSG_CAST(&ThisClass::OnPaletteChanged) },
#define ON_WM_CONTEXTMENU() \
    { WM_CONTEXTMENU, 0, 0, 0, AfxSig_vpP, AFX_PMSG_CAST(&ThisClass::OnContextMenu) },
#define ON_WM_ENTERIDLE() \
    { WM_ENTERIDLE, 0, 0, 0, AfxSig_vwp, AFX_PMSG_CAST(&ThisClass::OnEnterIdle) },
#define ON_WM_MENUSELECT() \
    { WM_MENUSELECT, 0, 0, 0, AfxSig_vwwh, AFX_PMSG_CAST(&ThisClass::OnMenuSelect) },
#define ON_WM_VSCROLL() \
    { WM_VSCROLL, 0, 0, 0, AfxSig_vwwx, AFX_PMSG_CAST(&ThisClass::OnVScroll) },
#define ON_WM_HSCROLL() \
    { WM_HSCROLL, 0, 0, 0, AfxSig_vwwx, AFX_PMSG_CAST(&ThisClass::OnHScroll) },
#define ON_WM_SHOWWINDOW() \
    { WM_SHOWWINDOW, 0, 0, 0, AfxSig_vbw, AFX_PMSG_CAST(&ThisClass::OnShowWindow) },
#define ON_WM_GETDLGCODE() \
    { WM_GETDLGCODE, 0, 0, 0, AfxSig_wv, AFX_PMSG_CAST(&ThisClass::OnGetDlgCode) },
#define ON_WM_ERASEBKGND() \
    { WM_ERASEBKGND, 0, 0, 0, AfxSig_bD, AFX_PMSG_CAST(&ThisClass::OnEraseBkgnd) },
#define ON_WM_MOUSEWHEEL() \
    { WM_MOUSEWHEEL, 0, 0, 0, AfxSig_bwsP, AFX_PMSG_CAST(&ThisClass::OnMouseWheel) },
#define ON_WM_CLOSE() \
    { WM_CLOSE, 0, 0, 0, AfxSig_vv, AFX_PMSG_CAST(&ThisClass::OnClose) },
#define ON_WM_MOVE() \
    { WM_MOVE, 0, 0, 0, AfxSig_unsupported, AFX_PMSG_CAST(&ThisClass::OnMove) },

// Present in the sources, not yet dispatched. Each occupies a valid slot; the dispatcher
// skips AfxSig_unsupported. Listed rather than left undefined so a map that uses one still
// compiles - a missing ON_WM_* macro does not fail at its own line, it makes the NEXT
// entry report "expected function body after function declarator", which is a genuinely
// confusing way to find out.
#define ON_WM_SETCURSOR() \
    { WM_SETCURSOR, 0, 0, 0, AfxSig_unsupported, AFX_PMSG_CAST(&ThisClass::OnSetCursor) },
#define ON_WM_SYSCOLORCHANGE() \
    { WM_SYSCOLORCHANGE, 0, 0, 0, AfxSig_unsupported, \
      AFX_PMSG_CAST(&ThisClass::OnSysColorChange) },
#define ON_WM_INITMENUPOPUP() \
    { WM_INITMENUPOPUP, 0, 0, 0, AfxSig_unsupported, \
      AFX_PMSG_CAST(&ThisClass::OnInitMenuPopup) },
#define ON_WM_MEASUREITEM() \
    { WM_MEASUREITEM, 0, 0, 0, AfxSig_unsupported, \
      AFX_PMSG_CAST(&ThisClass::OnMeasureItem) },
#define ON_WM_DRAWITEM() \
    { WM_DRAWITEM, 0, 0, 0, AfxSig_unsupported, AFX_PMSG_CAST(&ThisClass::OnDrawItem) },
#define ON_WM_WINDOWPOSCHANGING() \
    { WM_WINDOWPOSCHANGING, 0, 0, 0, AfxSig_unsupported, \
      AFX_PMSG_CAST(&ThisClass::OnWindowPosChanging) },
#define ON_WM_GETMINMAXINFO() \
    { WM_GETMINMAXINFO, 0, 0, 0, AfxSig_unsupported, \
      AFX_PMSG_CAST(&ThisClass::OnGetMinMaxInfo) },
#define ON_WM_ACTIVATE() \
    { WM_ACTIVATE, 0, 0, 0, AfxSig_unsupported, AFX_PMSG_CAST(&ThisClass::OnActivate) },
#define ON_WM_NCHITTEST() \
    { WM_NCHITTEST, 0, 0, 0, AfxSig_unsupported, AFX_PMSG_CAST(&ThisClass::OnNcHitTest) },
#define ON_WM_QUERYNEWPALETTE() \
    { WM_QUERYNEWPALETTE, 0, 0, 0, AfxSig_unsupported, \
      AFX_PMSG_CAST(&ThisClass::OnQueryNewPalette) },
#define ON_WM_HELPINFO() \
    { WM_HELP, 0, 0, 0, AfxSig_unsupported, AFX_PMSG_CAST(&ThisClass::OnHelpInfo) },
#define ON_WM_CANCELMODE() \
    { WM_CANCELMODE, 0, 0, 0, AfxSig_unsupported, \
      AFX_PMSG_CAST(&ThisClass::OnCancelMode) },

#endif // NATIVE_MSGMAP_H
