// mfcui.h - MFC's window/app/control classes as empty stubs.
//
// PURPOSE IS PARSING, NOT BEHAVIOUR. The engine's headers are one big include
// web: chat.h declares `class CChatApp : public CWinApp`, and any translation unit
// that reaches chat.h - which includes avbfile.cpp, avatar.cpp and backdrop.cpp,
// none of which touch the UI - needs CWinApp to be a complete type just to get
// past the declaration. Every class here exists for that reason alone.
//
// So: no method here does anything, and none is expected to be called. When the
// native app grows a real AppKit front end, it will NOT be by filling these in -
// the MFC view/dialog model does not map onto AppKit, and pretending it does would
// produce a worse app than writing NSView subclasses directly. These stubs are
// scaffolding for the ENGINE, and the front end will be new code that calls the
// engine, not a CView emulation.
//
// The one thing that must stay honest: anything the engine calls for a VALUE gets
// a stub only if the value is genuinely unobservable in the paths we run. Where
// that is not true, it belongs in gdishim.h with an abort, not here.

#ifndef NATIVE_SHIM_MFCUI_H
#define NATIVE_SHIM_MFCUI_H

#include "win32types.h"
#include "mfcshim.h"
#include "gdishim.h"
#include "richedit.h"
// CAsyncSocket's signatures use SOCKADDR, so this header must be self-sufficient in
// it. stdafx.h happens to include winsock.h too, but AFTER this file - relying on
// that order is the trap that has already cost several rounds here, since a type
// declared later in the chain breaks every TU at once.
#include "winsock.h"

// --- message maps: declarations vanish -------------------------------------
#define DECLARE_MESSAGE_MAP()
#define BEGIN_MESSAGE_MAP(cls, base)    /* the map body is dead code here; */
#define END_MESSAGE_MAP()               /* the macros below swallow it.    */
#define ON_COMMAND(id, fn)
#define ON_COMMAND_RANGE(a, b, fn)
#define ON_UPDATE_COMMAND_UI(id, fn)
#define ON_UPDATE_COMMAND_UI_RANGE(a, b, fn)
#define ON_NOTIFY(c, id, fn)
#define ON_NOTIFY_RANGE(c, a, b, fn)
#define ON_MESSAGE(msg, fn)
#define ON_REGISTERED_MESSAGE(msg, fn)
#define ON_WM_CREATE()
#define ON_WM_DESTROY()
#define ON_WM_PAINT()
#define ON_WM_SIZE()
#define ON_WM_TIMER()
#define ON_WM_LBUTTONDOWN()
#define ON_WM_LBUTTONUP()
#define ON_WM_LBUTTONDBLCLK()
#define ON_WM_RBUTTONDOWN()
#define ON_WM_RBUTTONUP()
#define ON_WM_MOUSEMOVE()
#define ON_WM_KEYDOWN()
#define ON_WM_KEYUP()
#define ON_WM_CHAR()
#define ON_WM_SETFOCUS()
#define ON_WM_KILLFOCUS()
#define ON_WM_CLOSE()
#define ON_WM_ERASEBKGND()
#define ON_WM_VSCROLL()
#define ON_WM_HSCROLL()
#define ON_WM_MOUSEWHEEL()
#define ON_WM_SETCURSOR()
#define ON_WM_CONTEXTMENU()
#define ON_WM_SYSCOLORCHANGE()
#define ON_WM_INITMENUPOPUP()
#define ON_WM_MEASUREITEM()
#define ON_WM_DRAWITEM()
#define ON_WM_WINDOWPOSCHANGING()
#define ON_WM_GETMINMAXINFO()
#define ON_WM_ACTIVATE()
#define ON_WM_NCHITTEST()
#define ON_WM_QUERYNEWPALETTE()
#define ON_WM_PALETTECHANGED()
#define ON_WM_HELPINFO()
#define ON_WM_SHOWWINDOW()
#define ON_WM_MOVE()
#define ON_WM_CANCELMODE()
// bodycam.cpp's map. A missing ON_WM_* macro does not fail where it is used - the line
// parses as a declaration and the error lands on the NEXT entry as "expected function
// body after function declarator", which is why these are worth keeping complete.
#define ON_WM_NCDESTROY()
#define ON_WM_GETDLGCODE()
#define ON_WM_MENUSELECT()
#define ON_WM_ENTERIDLE()
#define ON_EN_CHANGE(id, fn)
#define ON_BN_CLICKED(id, fn)
#define ON_CBN_SELCHANGE(id, fn)
#define ON_CBN_EDITCHANGE(id, fn)
#define ON_LBN_SELCHANGE(id, fn)
#define ON_BEGIN_OLECMD_MAP(cls, base)
#define BEGIN_OLECMD_MAP(cls, base)
#define END_OLECMD_MAP()
#define ON_OLECMD(g, id, cmd)
#define BEGIN_DISPATCH_MAP(cls, base)
#define END_DISPATCH_MAP()
#define DISP_FUNCTION(cls, name, fn, vt, params)
#define DISP_PROPERTY_EX(cls, name, get, set, vt)
#define DECLARE_DISPATCH_MAP()
#define DECLARE_OLECMD_MAP()
#define DECLARE_INTERFACE_MAP()
#define BEGIN_INTERFACE_MAP(cls, base)
#define END_INTERFACE_MAP()
#define INTERFACE_PART(cls, iid, part)
#define AFX_MANAGE_STATE(p)

// --- window styles, messages and box flags ---------------------------------
// Values are the real Win32 ones. They appear in style arguments and message
// comparisons inside headers the engine core includes; correct values cost
// nothing and a wrong one would be a silent behavioural difference if any of this
// is ever reached.
#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_CHILD            0x40000000L
#define WS_MINIMIZE         0x20000000L
#define WS_VISIBLE          0x10000000L
#define WS_DISABLED         0x08000000L
#define WS_CLIPSIBLINGS     0x04000000L
#define WS_CLIPCHILDREN     0x02000000L
#define WS_MAXIMIZE         0x01000000L
#define WS_BORDER           0x00800000L
#define WS_DLGFRAME         0x00400000L
#define WS_VSCROLL          0x00200000L
#define WS_HSCROLL          0x00100000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_GROUP            0x00020000L
#define WS_TABSTOP          0x00010000L
#define WS_CAPTION          (WS_BORDER | WS_DLGFRAME)
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX)
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L
#define WS_EX_CLIENTEDGE    0x00000200L
#define WS_EX_TOOLWINDOW    0x00000080L
#define WS_EX_TOPMOST       0x00000008L

#define ES_LEFT             0x0000L
#define ES_MULTILINE        0x0004L
#define ES_AUTOVSCROLL      0x0040L
#define ES_AUTOHSCROLL      0x0080L
#define ES_READONLY         0x0800L
#define ES_WANTRETURN       0x1000L

#define SW_HIDE             0
#define SW_SHOWNORMAL       1
// SW_NORMAL is the older spelling of SW_SHOWNORMAL; status.cpp uses it.
#define SW_NORMAL           1
#define SW_SHOW             5
#define SW_SHOWNA           8
#define SW_RESTORE          9

#define MB_OK               0x00000000L
#define MB_OKCANCEL         0x00000001L
#define MB_YESNO            0x00000004L
#define MB_ICONERROR        0x00000010L
#define MB_ICONQUESTION     0x00000020L
#define MB_ICONWARNING      0x00000030L
#define MB_ICONINFORMATION  0x00000040L
#define IDOK                1
#define IDCANCEL            2
#define IDYES               6
#define IDNO                7

#define WM_USER             0x0400
#define WM_COMMAND          0x0111
#define WM_NOTIFY           0x004E
#define WM_TIMER            0x0113
#define WM_PAINT            0x000F
#define WM_SIZE             0x0005
#define WM_CLOSE            0x0010
#define WM_DESTROY          0x0002
#define WM_SETFOCUS         0x0007
#define WM_KILLFOCUS        0x0008
#define WM_CHAR             0x0102
#define WM_KEYDOWN          0x0100
#define WM_LBUTTONDOWN      0x0201
#define WM_RBUTTONDOWN      0x0204
#define WM_MOUSEMOVE        0x0200
#define WM_SETREDRAW        0x000B
#define WM_UNDO             0x0304
#define WM_MENUSELECT       0x011F
#define WM_ENTERIDLE        0x0121
// Dialog-code bits returned from OnGetDlgCode: "send me all keys / the arrow keys rather
// than letting the dialog manager consume them".
#define DLGC_WANTARROWS     0x0001
#define DLGC_WANTTAB        0x0002
#define DLGC_WANTALLKEYS    0x0004
#define DLGC_WANTCHARS      0x0080
// Tooltip control style: always show, even when the owning window is inactive.
#define TTS_ALWAYSTIP       0x01
#define TTS_NOPREFIX        0x02
#define EM_GETSEL           0x00B0
#define EM_SETSEL           0x00B1
#define EM_CANUNDO          0x00C6
#define EM_UNDO             0x00C7
#define EM_REPLACESEL       0x00C2
#define EM_LINELENGTH       0x00C1
#define EM_LINEINDEX        0x00BB
#define EM_GETLINECOUNT     0x00BA
#define WM_CUT              0x0300
#define WM_COPY             0x0301
#define WM_PASTE            0x0302
#define WM_CLEAR            0x0303
#define WM_GETDLGCODE       0x0087
#define WM_VSCROLL          0x0115
#define WM_HSCROLL          0x0114

#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_NOACTIVATE      0x0010
#define SWP_SHOWWINDOW      0x0040

#define VK_RETURN           0x0D
#define VK_ESCAPE           0x1B
#define VK_TAB              0x09
#define VK_UP               0x26
#define VK_DOWN             0x28
#define VK_LEFT             0x25
#define VK_RIGHT            0x27
#define VK_PRIOR            0x21
#define VK_NEXT             0x22
#define VK_SHIFT            0x10
#define VK_CONTROL          0x11
#define VK_HOME             0x24
#define VK_END              0x23
#define VK_DELETE           0x2E
#define VK_BACK             0x08
#define VK_SPACE            0x20
#define VK_MENU             0x12

// Window long/style access. Reports 0 and accepts writes: there is no HWND behind
// any of this natively, and chatdoc.cpp uses it only to toggle the member-list view
// between LVS_ICON and LVS_REPORT.
#define GWL_STYLE           (-16)
#define GWL_EXSTYLE         (-20)
#define GWL_ID              (-12)
#define GWL_USERDATA        (-21)
#define LVS_ICON            0x0000
#define LVS_REPORT          0x0001
#define LVS_SMALLICON       0x0002
#define LVS_LIST            0x0003
#define LVS_TYPEMASK        0x0003
#define LVS_SINGLESEL       0x0004
#define LVS_SHOWSELALWAYS   0x0008
#define LVS_NOCOLUMNHEADER  0x4000
inline LONG GetWindowLong(HWND, int) { return 0; }
inline LONG SetWindowLong(HWND, int, LONG) { return 0; }

#define TRUE_WIN32          1

typedef struct _oracle_charrange { LONG cpMin, cpMax; } ORACLE_CHARRANGE;
typedef struct _findtextex {
    ORACLE_CHARRANGE chrg;
    LPCSTR lpstrText;
    ORACLE_CHARRANGE chrgText;
} FINDTEXTEX;
#define EM_FINDTEXTEX   (WM_USER + 79)
#define FR_DOWN         0x00000001
#define FR_WHOLEWORD    0x00000002
#define FR_MATCHCASE    0x00000004

// Common-control and dialog flags reached from headers.
#define OFN_HIDEREADONLY        0x00000004
#define OFN_EXPLORER            0x00080000
#define OFN_OVERWRITEPROMPT     0x00000002
#define OFN_FILEMUSTEXIST       0x00001000
#define OFN_PATHMUSTEXIST       0x00000800
#define OFN_NOCHANGEDIR         0x00000008
#define ES_SAVESEL              0x8000L
#define ES_NOHIDESEL            0x0100L
#define ES_DISABLENOSCROLL      0x2000L
#define CBRS_BORDER_TOP         0x00000001L
#define CBRS_BORDER_BOTTOM      0x00000002L
#define CBRS_BORDER_LEFT        0x00000004L
#define CBRS_BORDER_RIGHT       0x00000008L
#define CBRS_TOP                0x00000100L
#define CBRS_BOTTOM             0x00000200L
#define CBRS_ALIGN_ANY          0x00003C00L
#define CBRS_TOOLTIPS           0x00010000L
#define CBRS_FLYBY              0x00020000L
#define CBRS_SIZE_DYNAMIC       0x00040000L
#define CBRS_GRIPPER            0x00400000L

typedef struct _LV_ITEM {
    UINT   mask;
    int    iItem, iSubItem;
    UINT   state, stateMask;
    LPSTR  pszText;
    int    cchTextMax, iImage;
    LPARAM lParam;
} LV_ITEM, LVITEM, *LPLVITEM;
typedef struct _LV_COLUMN {
    UINT  mask;
    int   fmt, cx;
    LPSTR pszText;
    int   cchTextMax, iSubItem;
} LV_COLUMN, LVCOLUMN;
typedef struct _NM_LISTVIEW {
    NMHDR hdr;
    int   iItem, iSubItem;
    UINT  uNewState, uOldState, uChanged;
    POINT ptAction;
    LPARAM lParam;
} NM_LISTVIEW, NMLISTVIEW;
typedef struct _enlink { NMHDR nmhdr; UINT msg; WPARAM wParam; LPARAM lParam; ORACLE_CHARRANGE chrg; } ENLINK;
typedef struct _enprotected { NMHDR nmhdr; UINT msg; WPARAM wParam; LPARAM lParam; ORACLE_CHARRANGE chrg; } ENPROTECTED;

// CPrintInfo - printing is not part of the native app (print.cpp is dropped), but
// OnPreparePrinting/OnPrint overrides in pageview.h, panel.h and textview.h name it.
class CPrintInfo {
public:
    BOOL m_bPreview, m_bContinuePrinting, m_bDocObject;
    UINT m_nCurPage, m_nNumPreviewPages;
    CRect m_rectDraw;
    CPrintInfo() : m_bPreview(FALSE), m_bContinuePrinting(FALSE), m_bDocObject(FALSE),
                   m_nCurPage(1), m_nNumPreviewPages(0) {}
    void SetMaxPage(UINT) {}
    void SetMinPage(UINT) {}
    UINT GetMaxPage() const { return 1; }
    UINT GetMinPage() const { return 1; }
};

// --- the class hierarchy ---------------------------------------------------
class CCmdTarget : public CObject {
public:
    virtual ~CCmdTarget() {}
    void EnableAutomation() {}
};

// CCmdUI is the object passed to ON_UPDATE_COMMAND_UI handlers. Those handlers
// are declared in headers the engine core reaches; none runs natively.
class CDocument;   // CCreateContext holds one; declared in full further down.

// CCommandLineInfo / CCreateContext / CToolTipCtrl: the remaining MFC framework
// types named in engine headers. As above, declarations only.
class CCommandLineInfo : public CObject {
public:
    BOOL m_bShowSplash, m_bRunEmbedded, m_bRunAutomated;
    CString m_strFileName;
    CCommandLineInfo() : m_bShowSplash(TRUE), m_bRunEmbedded(FALSE), m_bRunAutomated(FALSE) {}
    virtual void ParseParam(LPCTSTR, BOOL, BOOL) {}
};

class CCreateContext {
public:
    CRuntimeClass* m_pNewViewClass;
    CDocument* m_pCurrentDoc;
    CCreateContext() : m_pNewViewClass(0), m_pCurrentDoc(0) {}
};

typedef struct tagTOOLINFO {
    UINT cbSize, uFlags;
    HWND hwnd;
    UINT_PTR uId;
    RECT rect;
    HINSTANCE hinst;
    LPSTR lpszText;
    LPARAM lParam;
} TOOLINFO, *LPTOOLINFO;

class CMenuFwd;
class CScrollBar;   // CWnd::OnVScroll/OnHScroll take one
class CFrameWnd;    // returned by CWnd::GetParentFrame
class CView;        // returned by CFrameWnd::GetActiveView
class CMDIChildWnd; // returned by CMDIFrameWnd::MDIGetActive

class CCmdUI {
public:
    UINT m_nID;
    // MFC exposes the menu being updated; chatdoc.cpp reaches for it to rebuild
    // submenus during ON_UPDATE_COMMAND_UI.
    class CMenu* m_pMenu;
    class CMenu* m_pSubMenu;
    UINT m_nIndex;
    CCmdUI() : m_nID(0), m_pMenu(0), m_pSubMenu(0), m_nIndex(0) {}
    void Enable(BOOL = TRUE) {}
    void SetCheck(int = 1) {}
    void SetRadio(BOOL = TRUE) {}
    void SetText(LPCTSTR) {}
};

class CWnd : public CCmdTarget {
public:
    HWND m_hWnd;
    CWnd() : m_hWnd(0) {}
    virtual ~CWnd() {}
    static CWnd* FromHandle(HWND) { return 0; }
    static CWnd* GetDesktopWindow() { return 0; }
    static CWnd* GetActiveWindow() { return 0; }
    static CWnd* GetFocus() { return 0; }
    HWND GetSafeHwnd() const { return m_hWnd; }
    BOOL IsWindow() const { return FALSE; }
    void GetClientRect(RECT* r) const { if (r) { r->left = r->top = r->right = r->bottom = 0; } }
    void GetWindowRect(RECT* r) const { if (r) { r->left = r->top = r->right = r->bottom = 0; } }
    void InvalidateRect(const RECT*, BOOL = TRUE) {}
    BOOL RedrawWindow(const RECT* = 0, CRgn* = 0, UINT = 0) { return TRUE; }
    virtual BOOL PreCreateWindow(CREATESTRUCT&) { return TRUE; }
    virtual BOOL PreTranslateMessage(MSG*) { return FALSE; }
    BOOL IsWindowVisible() const { return FALSE; }
    BOOL IsWindowEnabled() const { return FALSE; }
    // afx_msg handlers: NOT virtual, matching MFC. They are dispatched through the
    // message map, so a derived class's handler hides rather than overrides. Making
    // them virtual put every derived handler in a vtable that then demanded a
    // definition - CRtfCtrl's four, for one.
    void OnMouseMove(UINT, CPoint) {}
    void OnLButtonDown(UINT, CPoint) {}
    void OnLButtonUp(UINT, CPoint) {}
    void OnRButtonDown(UINT, CPoint) {}
    void OnRButtonUp(UINT, CPoint) {}
    int  OnCreate(LPCREATESTRUCT) { return 0; }
    void OnDestroy() {}
    // bodycam.cpp's overrides call the base implementation (CWnd::OnNcDestroy() and
    // CWnd::OnEnterIdle(...)), so these must exist as members, not just as map entries.
    void OnNcDestroy() {}
    // CWnd::Create - creating a real window. Reports failure; there is no window server
    // involvement in this build, and the AppKit shell will own its own windows rather than
    // going through CWnd.
    BOOL Create(LPCTSTR, LPCTSTR, DWORD, const RECT&, CWnd*, UINT, void* = 0) { return FALSE; }
    BOOL CreateEx(DWORD, LPCTSTR, LPCTSTR, DWORD, int, int, int, int, HWND, HMENU, void* = 0) { return FALSE; }
    // Mouse capture as a CWnd member. CWnd already declares IsWindow/IsWindowEnabled and
    // a static GetFocus above, so only SetCapture is missing.
    CWnd* SetCapture() { return 0; }
    void OnEnterIdle(UINT, CWnd*) {}
    void OnKeyDown(UINT, UINT, UINT) {}
    void OnKeyUp(UINT, UINT, UINT) {}
    void OnChar(UINT, UINT, UINT) {}
    void OnSetFocus(CWnd*) {}
    void OnKillFocus(CWnd*) {}
    void OnSize(UINT, int, int) {}
    void OnTimer(UINT) {}
    void OnVScroll(UINT, UINT, CScrollBar*) {}
    void OnHScroll(UINT, UINT, CScrollBar*) {}
    BOOL EnableToolTips(BOOL = TRUE) { return FALSE; }
    BOOL GetScrollInfo(int, void*, UINT = 0) { return FALSE; }
    BOOL SetScrollInfo(int, const void*, BOOL = TRUE) { return FALSE; }
    void Invalidate(BOOL = TRUE) {}
    void UpdateWindow() {}
    BOOL ShowWindow(int) { return TRUE; }
    void SetWindowText(LPCTSTR) {}
    int GetWindowText(LPTSTR, int) const { return 0; }
    void GetWindowText(CString& s) const { s.Empty(); }
    CWnd* GetParent() const { return 0; }
    CWnd* GetDlgItem(int) const { return 0; }
    CMenu* GetMenu() const { return 0; }
    CFrameWnd* GetParentFrame() const { return 0; }
    CWnd* GetTopLevelParent() const { return 0; }
    void DrawMenuBar() {}
    void SetFocus() {}
    BOOL EnableWindow(BOOL = TRUE) { return TRUE; }
    UINT SetTimer(UINT, UINT, void*) { return 0; }
    BOOL KillTimer(UINT) { return TRUE; }
    LONG SendMessage(UINT, UINT = 0, LONG = 0) { return 0; }
    BOOL PostMessage(UINT, UINT = 0, LONG = 0) { return TRUE; }
    void SetRedraw(BOOL = TRUE) {}
    void ScreenToClient(POINT*) const {}
    void ClientToScreen(POINT*) const {}
    CDC* GetDC() { return 0; }
    int ReleaseDC(CDC*) { return 0; }
    void MoveWindow(int, int, int, int, BOOL = TRUE) {}
    void SetWindowPos(const CWnd*, int, int, int, int, UINT) {}
};

class CFrameWnd : public CWnd {
public:
    void ActivateFrame(int = -1) {}
    CView* GetActiveView() const { return 0; }
};
class CMDIFrameWnd : public CFrameWnd {
public:
    CMDIChildWnd* MDIGetActive(BOOL* = 0) const { return 0; }
};
class CMDIChildWnd : public CFrameWnd {};
class CMiniFrameWnd : public CFrameWnd {};
class CSplitterWnd : public CWnd {};
class CControlBar : public CWnd {};
class CToolBar : public CControlBar {
public:
    BOOL LoadToolBar(UINT) { return FALSE; }
    BOOL SetButtons(const UINT*, int) { return FALSE; }
    void SetSizes(SIZE, SIZE) {}
};

// CCoolBar / CCoolToolBar are the project's toolbar classes (coolbar.h), stubbed
// alongside CCSDialog and CCSPropertyPage for the same reason.
class CCoolBar : public CControlBar {};
class CCoolToolBar : public CToolBar {};
// NOTE: CCoolToolBarEx is defined by the project's chatbars.h - not stubbed here.

#define AFX_IDW_TOOLBAR         0xE800
#define AFX_IDW_STATUS_BAR      0xE801
#define AFX_IDW_PANE_FIRST      0xE900
#define AFX_IDW_PANE_LAST       0xE9FF
#define CBRS_ALIGN_TOP          0x00000100L
#define CBRS_ALIGN_BOTTOM       0x00000200L
#define CBRS_ALIGN_LEFT         0x00000400L
#define CBRS_ALIGN_RIGHT        0x00000800L
#define CB_ERR                  (-1)
#define ID_FILE_NEW             0xE100

// Menu-item flags and mouse-key modifiers.
#define MF_INSERT               0x00000000L
#define MF_CHANGE               0x00000080L
#define MF_APPEND               0x00000100L
#define MF_DELETE               0x00000200L
#define MF_REMOVE               0x00001000L
#define MF_BYCOMMAND            0x00000000L
#define MF_BYPOSITION           0x00000400L
#define MF_SEPARATOR            0x00000800L
#define MF_ENABLED              0x00000000L
#define MF_GRAYED               0x00000001L
#define MF_DISABLED             0x00000002L
#define MF_UNCHECKED            0x00000000L
#define MF_CHECKED              0x00000008L
#define MF_STRING               0x00000000L
#define MF_POPUP                0x00000010L
#define MK_LBUTTON              0x0001
#define MK_RBUTTON              0x0002
#define MK_SHIFT                0x0004
#define MK_CONTROL              0x0008
#define WHEEL_DELTA             120
#define TPM_LEFTALIGN           0x0000L
#define TPM_RIGHTALIGN          0x0008L
#define TPM_LEFTBUTTON          0x0000L
#define TPM_RIGHTBUTTON         0x0002L
#define SC_RESTORE              0xF120
#define SC_MINIMIZE             0xF020
#define SC_MAXIMIZE             0xF030
#define SC_CLOSE                0xF060
class CStatusBar : public CControlBar {};
class CDialogBar : public CControlBar {};

class CDialog : public CWnd {
public:
    CDialog() {}
    CDialog(UINT, CWnd* = 0) {}
    CDialog(LPCTSTR, CWnd* = 0) {}
    virtual int DoModal() { return 0; }
    virtual BOOL OnInitDialog() { return TRUE; }
    virtual void OnOK() {}
    virtual void OnCancel() {}
    virtual void DoDataExchange(void*) {}
    BOOL UpdateData(BOOL = TRUE) { return TRUE; }
    void EndDialog(int) {}
    // Keyboard-focus movement within the dialog. No-ops: there is no dialog and no
    // focus. ircsock.cpp calls these on the room list after repopulating it.
    void GotoDlgCtrl(CWnd*) {}
    void NextDlgCtrl() const {}
    void PrevDlgCtrl() const {}
};

// CCSDialog / CCoolBar are PROJECT classes (chicdial.h, coolbar.h) that the
// engine's own stdafx.h pulls in, so headers like userlist.h derive from them
// without including them. Stubbed here rather than including those headers: they
// are UI code, and pulling them into the platform floor drags a large Win32
// surface (LPCREATESTRUCT, toolbar notifications) into all 92 translation units
// for the benefit of two base-class names.
class CCSDialog : public CDialog {
public:
    CCSDialog() {}
    CCSDialog(UINT id, CWnd* p = 0) : CDialog(id, p) {}
};

class CPropertyPage : public CDialog {
public:
    CPropertyPage() {}
    CPropertyPage(UINT, UINT = 0) {}
    void SetModified(BOOL = TRUE) {}
};

// CCSPropertyPage is the project's property-page base (chicdial.h), stubbed here
// alongside CCSDialog and for the same reason: setupdlg.h and friends derive from
// it, and pulling in the real UI headers drags a large Win32 surface into every
// translation unit.
class CCSPropertyPage : public CPropertyPage {
public:
    CCSPropertyPage() {}
    CCSPropertyPage(UINT id, UINT caption = 0) : CPropertyPage(id, caption) {}
};
class CPropertySheet : public CWnd {
public:
    CPropertySheet() {}
    CPropertySheet(LPCTSTR, CWnd* = 0, UINT = 0) {}
    void AddPage(CPropertyPage*) {}
    virtual int DoModal() { return 0; }
};

class CView : public CWnd {
public:
    // Protected in MFC; pageview.h reads it directly.
    CDocument* m_pDocument;
    CView() : m_pDocument(0) {}
    virtual void OnDraw(CDC*) {}
    virtual void OnInitialUpdate() {}
    virtual void OnActivateView(BOOL, CView*, CView*) {}
    virtual void OnUpdate(CView*, LPARAM, CObject*) {}
    virtual BOOL OnPreparePrinting(CPrintInfo*) { return TRUE; }
    BOOL DoPreparePrinting(CPrintInfo*) { return TRUE; }
    virtual void OnBeginPrinting(CDC*, CPrintInfo*) {}
    virtual void OnEndPrinting(CDC*, CPrintInfo*) {}
    virtual void OnPrint(CDC*, CPrintInfo*) {}
    virtual void OnPrepareDC(CDC*, CPrintInfo* = 0) {}
    class CDocument* GetDocument() const { return 0; }
};
class CScrollView : public CView {
public:
    // MFC keeps the view's mapping mode here (NOT on CDC, where an earlier version
    // of this shim wrongly put it - pageview.cpp reads it as a view member).
    int m_nMapMode;
    // MFC's scroll bookkeeping, all read directly by pageview.cpp. Zeroed: there is
    // no window, so there is nothing to scroll - and pageview only reads them to
    // clamp against, so zeros keep it inside bounds rather than sending it off the
    // end.
    CSize m_totalLog, m_totalDev, m_pageDev, m_lineDev;
    CScrollView() : m_nMapMode(MM_TEXT) {}
    virtual void OnInitialUpdate() {}
    void SetScrollSizes(int, SIZE, SIZE = CSize(0,0), SIZE = CSize(0,0)) {}
    void SetScaleToFitSize(SIZE) {}
    CPoint GetScrollPosition() const { return CPoint(0, 0); }
    CPoint GetDeviceScrollPosition() const { return CPoint(0, 0); }
    void ScrollToPosition(POINT) {}
    void ScrollToDevicePosition(POINT) {}
    void UpdateBars() {}
    BOOL OnScrollBy(SIZE, BOOL = TRUE) { return FALSE; }
    BOOL OnScroll(UINT, UINT, BOOL = TRUE) { return FALSE; }
    int GetScrollPos(int) const { return 0; }
    int GetScrollLimit(int) const { return 0; }
    SIZE GetTotalSize() const { CSize s(0, 0); return s; }
};

class CDocTemplate;   // returned by CDocument::GetDocTemplate

class CDocument : public CCmdTarget {
public:
    // m_strPathName is protected in MFC and chatdoc.h touches it directly, so it
    // has to be a real member rather than hidden behind GetPathName().
    CString m_strPathName;
    CString m_strTitle;
    virtual ~CDocument() {}
    virtual BOOL SaveModified() { return TRUE; }
    virtual BOOL OnNewDocument() { return TRUE; }
    virtual BOOL OnOpenDocument(LPCTSTR) { return TRUE; }
    virtual BOOL OnSaveDocument(LPCTSTR) { return TRUE; }
    virtual void OnCloseDocument() {}
    void OnFileClose() {}
    // MFC's list of attached views; chatdoc.cpp walks it. Empty, since no view is
    // ever attached in a dump.
    CPtrList m_viewList;
    virtual void DeleteContents() {}
    void SetModifiedFlag(BOOL = TRUE) {}
    void UpdateAllViews(CView*, LONG = 0, CObject* = 0) {}
    CString GetPathName() const { return m_strPathName; }
    CDocTemplate* GetDocTemplate() const { return 0; }
    CString GetTitle() const { return m_strTitle; }
    void SetTitle(LPCTSTR t) { m_strTitle = t ? t : ""; }
    void SetPathName(LPCTSTR p, BOOL = TRUE) { m_strPathName = p ? p : ""; }
};
class COleServerItem;   // returned by COleServerDoc::GetEmbeddedItem below
class COleDocument : public CDocument {};
class COleServerDoc : public COleDocument {
public:
    // MFC member; chatdoc.cpp assigns it directly in its constructor.
    BOOL m_bRemember;
    COleServerDoc() : m_bRemember(TRUE) {}
    // NOT virtual - matching MFC. chatdoc.h declares `CChatItem* GetEmbeddedItem()`
    // which HIDES this rather than overriding it; making it virtual here turned
    // that into a covariant-return override and required CChatItem to be complete,
    // which it is not at that point. Only OnGetEmbeddedItem is virtual in MFC.
    COleServerItem* GetEmbeddedItem() { return 0; }
    virtual COleServerItem* OnGetEmbeddedItem() { return 0; }
    virtual void NotifyChanged() {}
    virtual void NotifySaved() {}
    virtual void NotifyClosed() {}
    virtual void ReportSaveLoadException(LPCTSTR, CException*, BOOL, UINT) {}
    virtual void OnFrameWindowActivate(BOOL) {}
};
class COleServerItem : public CCmdTarget {
public:
    CDocument* GetDocument() const { return 0; }
};

// CWaitCursor is MFC's RAII busy-cursor. Nothing to do without a window; kept as a
// type so the `CWaitCursor wait;` declarations compile.
class CWaitCursor {
public:
    CWaitCursor() {}
    ~CWaitCursor() {}
    void Restore() {}
};

class CWinApp : public CCmdTarget {
public:
    LPCTSTR m_pszAppName;
    HINSTANCE m_hInstance;
    CWnd* m_pMainWnd;
    CWinApp() : m_pszAppName("chat"), m_hInstance(0), m_pMainWnd(0) {}
    virtual ~CWinApp() {}
    virtual BOOL InitInstance() { return TRUE; }
    virtual int ExitInstance() { return 0; }
    UINT GetProfileInt(LPCTSTR, LPCTSTR, int def) { return (UINT)def; }
    CString GetProfileString(LPCTSTR, LPCTSTR, LPCTSTR def = 0) { return CString(def ? def : ""); }
    BOOL WriteProfileInt(LPCTSTR, LPCTSTR, int) { return TRUE; }
    BOOL WriteProfileString(LPCTSTR, LPCTSTR, LPCTSTR) { return TRUE; }
    void AddDocTemplate(void*) {}
};

// --- controls --------------------------------------------------------------
class CStatic : public CWnd {};
class CButton : public CWnd {
public:
    void SetCheck(int) {}
    int GetCheck() const { return 0; }
};
class CEdit : public CWnd {
public:
    int LineLength(int = -1) const { return 0; }
    int LineIndex(int = -1) const { return 0; }
    int LineFromChar(int = -1) const { return 0; }
    int GetLine(int, LPTSTR, int) const { return 0; }
    void SetSel(int, int, BOOL = FALSE) {}
    void GetSel(int&, int&) const {}
    void ReplaceSel(LPCTSTR, BOOL = FALSE) {}
    int GetLineCount() const { return 0; }
    void LimitText(int) {}
};
class CListBox : public CWnd {
public:
    int AddString(LPCTSTR) { return 0; }
    int GetCount() const { return 0; }
    int GetCurSel() const { return -1; }
    void ResetContent() {}
};
class CComboBox : public CWnd {
public:
    int AddString(LPCTSTR) { return 0; }
    int GetCount() const { return 0; }
    int GetCurSel() const { return -1; }
    int SetCurSel(int) { return -1; }
    void ResetContent() {}
    int GetLBTextLen(int) const { return 0; }
    void GetLBText(int, CString&) const {}
    int SetItemData(int, DWORD) { return 0; }
    DWORD GetItemData(int) const { return 0; }
    void* GetItemDataPtr(int) const { return 0; }
    int SetItemDataPtr(int, void*) { return 0; }
};
#define LVNI_ALL        0x0000
#define LVNI_SELECTED   0x0002
#define LVNI_FOCUSED    0x0001
#define LVIS_SELECTED   0x0002
#define LVIS_FOCUSED    0x0001
#define LVIS_STATEIMAGEMASK 0xF000
#define INDEXTOSTATEIMAGEMASK(i)  ((i) << 12)
#define LVIF_TEXT       0x0001
#define LVIF_IMAGE      0x0002
#define LVIF_PARAM      0x0004
#define LVIF_STATE      0x0008
#define LVIF_INDENT     0x0010
#define LVFI_PARAM      0x0001
#define LVFI_STRING     0x0002
#define LVFI_PARTIAL    0x0008
#define LVFI_WRAP       0x0020
#define LB_ERR          (-1)
#define LPSTR_TEXTCALLBACK  ((LPSTR)(intptr_t)-1)
#define I_IMAGECALLBACK     (-1)
#define LVCF_FMT        0x0001
#define LVCF_WIDTH      0x0002
#define LVCF_TEXT       0x0004
#define LVCF_SUBITEM    0x0008
#define LVCFMT_LEFT     0x0000
#define LVCFMT_RIGHT    0x0001
#define LVCFMT_CENTER   0x0002
#define LVSIL_SMALL     1
#define LVSIL_NORMAL    0
#define LVSIL_STATE     2

// Scroll-bar identifiers and clipboard formats.
#define SB_HORZ         0
#define SB_VERT         1
#define SB_CTL          2
#define SB_BOTH         3
#define SB_LINEUP       0
#define SB_LINEDOWN     1
#define SB_PAGEUP       2
#define SB_PAGEDOWN     3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK   5
#define SB_TOP          6
#define SB_BOTTOM       7
#define CF_TEXT         1
#define CF_BITMAP       2
#define CF_OEMTEXT      7

// Clipboard. Stubs that report failure: the engine treats a failed OpenClipboard as
// "cannot copy right now" and skips the operation, which is the correct first-run
// behaviour. Native clipboard support belongs on NSPasteboard behind a small
// interface, not here.
inline BOOL OpenClipboard(HWND) { return FALSE; }
inline BOOL CloseClipboard() { return TRUE; }
inline BOOL EmptyClipboard() { return TRUE; }
inline HANDLE GetClipboardData(UINT) { return (HANDLE)0; }
inline HANDLE SetClipboardData(UINT, HANDLE) { return (HANDLE)0; }
inline BOOL IsClipboardFormatAvailable(UINT) { return FALSE; }
#define LVIS_OVERLAYMASK    0x0F00

class CListCtrl : public CWnd {
public:
    int GetNextItem(int, int) const { return -1; }
    UINT GetItemState(int, UINT) const { return 0; }
    BOOL SetItemState(int, UINT, UINT) { return TRUE; }
    BOOL DeleteItem(int) { return TRUE; }
    BOOL RedrawItems(int, int) { return TRUE; }
    BOOL GetItem(LV_ITEM*) const { return FALSE; }
    BOOL SetItem(const LV_ITEM*) { return FALSE; }
    int FindItem(void*, int = -1) const { return -1; }
    void* SetImageList(void*, int) { return 0; }
    int InsertColumn(int, const LV_COLUMN*) { return -1; }
    BOOL SetColumnWidth(int, int) { return TRUE; }
    BOOL EnsureVisible(int, BOOL) { return TRUE; }
    CString GetItemText(int, int) const { return CString(); }
    BOOL SetItemText(int, int, LPCTSTR) { return TRUE; }
    int GetItemCount() const { return 0; }
    UINT GetSelectedCount() const { return 0; }
    BOOL DeleteAllItems() { return TRUE; }
    int InsertItem(const void*) { return 0; }
    DWORD GetItemData(int) const { return 0; }
    BOOL SetItemData(int, DWORD) { return TRUE; }
};
class CTreeCtrl : public CWnd {};
class CSpinButtonCtrl : public CWnd {
public:
    int SetRange(int, int) { return 0; }
    int SetPos(int) { return 0; }
    int GetPos() const { return 0; }
    CWnd* SetBuddy(CWnd*) { return 0; }
};
class CScrollBar : public CWnd {
public:
    int SetScrollPos(int, BOOL = TRUE) { return 0; }
    int GetScrollPos() const { return 0; }
    int GetScrollLimit() const { return 0; }
    void SetScrollRange(int, int, BOOL = TRUE) {}
    BOOL SetScrollInfo(void*, BOOL = TRUE) { return TRUE; }
};

typedef struct tagACCEL { BYTE fVirt; WORD key, cmd; } ACCEL, *LPACCEL;
struct AFX_CMDHANDLERINFO { void* pTarget; void* pmf; };

class CToolBarCtrl : public CWnd {};

class CToolTipCtrl : public CWnd {
public:
    BOOL Create(CWnd*, DWORD = 0) { return FALSE; }
    BOOL AddTool(CWnd*, LPCTSTR, LPCRECT = 0, UINT_PTR = 0) { return FALSE; }
    void Activate(BOOL) {}
    BOOL GetToolInfo(TOOLINFO&, CWnd*, UINT_PTR = 0) const { return FALSE; }
    void UpdateTipText(LPCTSTR, CWnd*, UINT_PTR = 0) {}
};
class CTabCtrl : public CWnd {};
class CProgressCtrl : public CWnd {};
class CSliderCtrl : public CWnd {};
// CRichEditCtrl - format.cpp drives one to convert between RTF and plain text via
// the clipboard. That path is Win32-specific in a way no stub can fake, so these
// exist to compile the translation unit; the native app will need its own RTF
// handling (or none - the comic view does not use rich text, only the text view
// does).
class CRichEditCtrl : public CWnd {
public:
    BOOL Create(DWORD, const RECT&, CWnd*, UINT) { return FALSE; }
    void SetSel(long, long) {}
    void SetSel(ORACLE_CHARRANGE) {}
    void GetSel(ORACLE_CHARRANGE&) const {}
    BOOL GetSelectionCharFormat(CHARFORMAT&) const { return FALSE; }
    BOOL SetSelectionCharFormat(CHARFORMAT&) { return FALSE; }
    BOOL SetDefaultCharFormat(CHARFORMAT&) { return FALSE; }
    BOOL SetParaFormat(PARAFORMAT&) { return FALSE; }
    CString GetSelText() const { return CString(); }
    long StreamIn(int, void*) { return 0; }
    long StreamOut(int, void*) { return 0; }
    void ReplaceSel(LPCTSTR) {}
    long GetTextLength() const { return 0; }
    int LineLength(int = -1) const { return 0; }
    int LineIndex(int = -1) const { return 0; }
    int LineFromChar(int = -1) const { return 0; }
    int GetLineCount() const { return 0; }
    int GetLine(int, LPTSTR, int) const { return 0; }
    void Copy() {}
    void Paste() {}
    void Clear() {}
    long FindText(DWORD, void*) const { return -1; }
};

// CCommonDialog is MFC's base for the system dialogs; utils.h derives
// CBrowseFolderDialog from it.
class CCommonDialog : public CDialog {
public:
    CCommonDialog(CWnd* p = 0) : CDialog((UINT)0, p) {}
};

typedef struct tagOPENFILENAME {
    DWORD  lStructSize;
    HWND   hwndOwner;
    HINSTANCE hInstance;
    LPCSTR lpstrFilter, lpstrCustomFilter;
    DWORD  nMaxCustFilter, nFilterIndex;
    LPSTR  lpstrFile;
    DWORD  nMaxFile;
    LPSTR  lpstrFileTitle;
    DWORD  nMaxFileTitle;
    LPCSTR lpstrInitialDir, lpstrTitle;
    DWORD  Flags;
    WORD   nFileOffset, nFileExtension;
    LPCSTR lpstrDefExt;
    LPARAM lCustData;
    void*  lpfnHook;
    LPCSTR lpTemplateName;
} OPENFILENAME;

class CFileDialog : public CCommonDialog {
public:
    // MFC exposes the raw OPENFILENAME so callers can tweak flags; chatdoc.cpp does.
    OPENFILENAME m_ofn;
    CString GetFileExt() const { return CString(); }
    CFileDialog(BOOL, LPCTSTR = 0, LPCTSTR = 0, DWORD = 0, LPCTSTR = 0, CWnd* = 0) {}
    CString GetPathName() const { return CString(); }
    CString GetFileName() const { return CString(); }
};
class CFontDialog : public CCommonDialog {
public:
    CFontDialog() {}
    CFontDialog(LOGFONT*, DWORD = 0, CDC* = 0, CWnd* = 0) {}
};
class CMenu : public CObject {
public:
    // m_hMenu is public in MFC and chatdoc.cpp reads it directly when reparenting
    // the in-place menu. Always NULL here: there is no menu bar.
    HMENU m_hMenu;
    CMenu() : m_hMenu(0) {}
    void Attach(HMENU h) { m_hMenu = h; }
    HMENU Detach() { HMENU h = m_hMenu; m_hMenu = 0; return h; }
    BOOL RemoveMenu(UINT, UINT) { return TRUE; }
    UINT GetMenuState(UINT, UINT) const { return (UINT)-1; }
    BOOL LoadMenu(UINT) { return FALSE; }
    BOOL CreatePopupMenu() { return FALSE; }
    BOOL DestroyMenu() { return TRUE; }
    BOOL TrackPopupMenu(UINT, int, int, CWnd*, const RECT* = 0) { return FALSE; }
    UINT GetMenuItemCount() const { return 0; }
    UINT GetMenuItemID(int) const { return 0; }
    BOOL DeleteMenu(UINT, UINT) { return TRUE; }
    BOOL InsertMenu(UINT, UINT, UINT = 0, LPCTSTR = 0) { return TRUE; }
    BOOL ModifyMenu(UINT, UINT, UINT = 0, LPCTSTR = 0) { return TRUE; }
    CMenu* GetSubMenu(int) { return 0; }
    BOOL AppendMenu(UINT, UINT = 0, LPCTSTR = 0) { return TRUE; }
    UINT CheckMenuItem(UINT, UINT) { return 0; }
    UINT EnableMenuItem(UINT, UINT) { return 0; }
};

// --- OLE / automation -------------------------------------------------------
// The native app drops ActiveX embedding entirely (bind*, oleobjct, chatitem,
// chatsrv are not being ported), but chat.h declares a COleTemplateServer member
// and the bind* headers derive from these, so the names must resolve for any
// translation unit that reaches chat.h. Nothing here is ever constructed.
class COleTemplateServer : public CCmdTarget {
public:
    void ConnectTemplate(REFCLSID, void*, BOOL) {}
    void UpdateRegistry(int = 0) {}
};
class COleObjectFactory : public CCmdTarget {};
class CDocTemplate : public CCmdTarget {
public:
    // MFC keeps the in-place server menu handle here; chatdoc.cpp swaps it while
    // embedded. NULL, since nothing is embedded.
    HMENU m_hMenuInPlaceServer;
    CDocTemplate() : m_hMenuInPlaceServer(0) {}
};
class CSingleDocTemplate : public CDocTemplate {
public:
    CSingleDocTemplate(UINT, CRuntimeClass*, CRuntimeClass*, CRuntimeClass*) {}
};
class CMultiDocTemplate : public CDocTemplate {
public:
    CMultiDocTemplate(UINT, CRuntimeClass*, CRuntimeClass*, CRuntimeClass*) {}
};
class COleDispatchDriver {};
class CAsyncMonikerFile : public CObject {};
class COleControl : public CWnd {};
class COleClientItem : public CCmdTarget {};
class COleDocObjectItem : public COleClientItem {};
// CDocObjectServerDoc / CDocObjectServerItem / CDocObjectServer are NOT stubbed
// here: the engine ships its own implementations in binddoc.h, binditem.h and
// bindipfw.h, and defining them here too is a redefinition error.
class COleIPFrameWnd : public CFrameWnd {
public:
    void SetActiveView(CView*, BOOL = TRUE) {}
};
class COleDocIPFrameWnd : public COleIPFrameWnd {};

inline BOOL AfxOleRegisterServerClass(REFCLSID, LPCTSTR, LPCTSTR, LPCTSTR) { return TRUE; }
inline void AfxOleSetUserCtrl(BOOL) {}
inline BOOL AfxOleCanExitApp() { return TRUE; }
inline void AfxOleLockApp() {}
inline void AfxOleUnlockApp() {}

// --- sockets ---------------------------------------------------------------
// The engine's own chatsock replaces most of this; CAsyncSocket appears only as a
// base class in a header. Real networking will use BSD sockets directly.
class CAsyncSocket : public CObject {
public:
    // MFC exposes the raw socket; protsupp.cpp reads it to poll connection state.
    // INVALID_SOCKET, since nothing is connected.
    int m_hSocket;   // SOCKET; spelled int because winsock.h comes after this header
    CAsyncSocket() : m_hSocket(-1) {}   // INVALID_SOCKET
    virtual ~CAsyncSocket() {}
    virtual void OnReceive(int) {}
    virtual void OnSend(int) {}
    virtual void OnConnect(int) {}
    virtual void OnClose(int) {}
    BOOL Create(UINT = 0, int = 0, long = 0, LPCTSTR = 0) { return FALSE; }
    BOOL Connect(LPCTSTR, UINT) { return FALSE; }
    int Send(const void*, int, int = 0) { return -1; }
    int Receive(void*, int, int = 0) { return -1; }
    void Close() {}
    // GetSockName/GetPeerName report failure: nothing is connected, and ircproto.cpp
    // treats a failure as "no local address yet" and returns 0 rather than proceeding
    // with garbage.
    BOOL GetSockName(SOCKADDR*, int*) { return FALSE; }
    BOOL GetPeerName(SOCKADDR*, int*) { return FALSE; }
    BOOL AsyncSelect(long = 0) { return FALSE; }
    BOOL IOCtl(long, DWORD*) { return FALSE; }
    BOOL ShutDown(int = 0) { return FALSE; }
    BOOL Listen(int = 5) { return FALSE; }
    BOOL Bind(UINT, LPCTSTR = 0) { return FALSE; }
    BOOL Accept(CAsyncSocket&, SOCKADDR* = 0, int* = 0) { return FALSE; }
};

// --- Win32 API surface reached from headers --------------------------------
// All stubs. These appear in dialog/UI headers that the engine core includes
// transitively; none is on a code path the native build runs. Grouped here rather
// than in win32types.h because they are UI-adjacent, not part of the type floor.
#define IMAGE_ICON              1
#define IMAGE_BITMAP            0
#define DI_NORMAL               0x0003
#define LR_DEFAULTCOLOR         0x0000
#define LR_LOADFROMFILE         0x0010
#define BIF_RETURNONLYFSDIRS    0x0001
#define CF_SCREENFONTS          0x00000001
#define CF_EFFECTS              0x00000100
#define CF_INITTOLOGFONTSTRUCT  0x00000040
#define SM_CXSCREEN             0
#define SM_CYSCREEN             1

typedef void* HKEY;
typedef void* LPITEMIDLIST;
typedef struct tagHELPINFO { UINT cbSize; int iContextType, iCtrlId; HANDLE hItemHandle; DWORD_PTR dwContextId; POINT MousePos; } HELPINFO, *LPHELPINFO;
typedef struct tagBROWSEINFO {
    HWND   hwndOwner;
    LPITEMIDLIST pidlRoot;
    LPSTR  pszDisplayName;
    LPCSTR lpszTitle;
    UINT   ulFlags;
    void*  lpfn;
    LPARAM lParam;
    int    iImage;
} BROWSEINFO, *LPBROWSEINFO;

class CImageList : public CObject {
public:
    BOOL Create(int, int, UINT, int, int) { return FALSE; }
    int  Add(CBitmap*, COLORREF) { return -1; }
    int  Add(CBitmap*, CBitmap*) { return -1; }   // image + mask overload
    HICON ExtractIcon(int) { return (HICON)0; }
};

inline HICON LoadIcon(HINSTANCE, LPCTSTR) { return (HICON)0; }
inline HANDLE LoadImage(HINSTANCE, LPCTSTR, UINT, int, int, UINT) { return (HANDLE)0; }
inline BOOL DestroyIcon(HICON) { return TRUE; }
inline BOOL DrawIcon(HDC, int, int, HICON) { return TRUE; }
inline BOOL DrawIconEx(HDC, int, int, HICON, int, int, UINT, HBRUSH, UINT) { return TRUE; }
inline int  GetSystemMetrics(int) { return 0; }
inline DWORD GetTickCount() { return 0; }
inline LPITEMIDLIST SHBrowseForFolder(LPBROWSEINFO) { return (LPITEMIDLIST)0; }
inline BOOL SHGetPathFromIDList(LPITEMIDLIST, LPSTR) { return FALSE; }

// --- free functions --------------------------------------------------------
inline CWinApp* AfxGetApp() { return 0; }
inline CWnd* AfxGetMainWnd() { return 0; }
inline BOOL IsWindow(HWND) { return FALSE; }
// Focus and capture as GLOBALS, distinct from the CWnd members: bodycam.cpp calls both
// spellings, and ::SetFocus takes the window it is restoring focus to.
inline HWND SetFocus(HWND) { return (HWND)0; }
inline BOOL ReleaseCapture() { return FALSE; }
inline HWND GetCapture() { return (HWND)0; }
inline LPSTR CharPrev(LPCSTR start, LPCSTR cur) { return (LPSTR)(cur > start ? cur - 1 : start); }

// Cursor and shell entry points, reached from urlutil.cpp's URL launcher. All inert:
// the native build has no cursor to set, and launching a URL should go through
// NSWorkspace rather than an emulated ShellExecute. Reporting failure (< 32 is the
// ShellExecute error convention) keeps the caller's error path intact.
#define IDC_ARROW           ((LPCSTR)(uintptr_t)32512)
#define IDC_WAIT            ((LPCSTR)(uintptr_t)32514)
#define IDC_APPSTARTING     ((LPCSTR)(uintptr_t)32650)
#define SW_SHOWNOACTIVATE   4
inline HCURSOR LoadCursor(HINSTANCE, LPCSTR) { return (HCURSOR)0; }
inline HCURSOR SetCursor(HCURSOR) { return (HCURSOR)0; }
inline HINSTANCE ShellExecute(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, int) {
    return (HINSTANCE)(uintptr_t)31;   // < 32 == failure, per ShellExecute's contract
}
inline UINT GetWindowsDirectory(LPSTR buf, UINT n) { if (buf && n) buf[0] = 0; return 0; }
inline UINT GetSystemDirectory(LPSTR buf, UINT n) { if (buf && n) buf[0] = 0; return 0; }
inline int AfxMessageBox(LPCTSTR, UINT = 0, UINT = 0) { return 0; }
inline int AfxMessageBox(UINT, UINT = 0, UINT = 0) { return 0; }
// Global ::SendMessage, distinct from CWnd::SendMessage. format.cpp posts rich-edit
// messages through it.
inline LRESULT SendMessage(HWND, UINT, WPARAM = 0, LPARAM = 0) { return 0; }
// CharNext / IsDBCSLeadByte: the MBCS text-walking pair. Single-byte behaviour here,
// matching the CP-1252 build the oracle covers; the CJK double-byte path is the
// deferred Tier-1 #13 scope question, and getting it wrong silently would corrupt
// multibyte text - so this advances one byte and reports no lead bytes rather than
// pretending to handle DBCS.
inline LPSTR CharNext(LPCSTR p) { return (LPSTR)(p && *p ? p + 1 : p); }
inline BOOL IsDBCSLeadByte(BYTE) { return FALSE; }
inline BOOL AfxOleInit() { return TRUE; }
inline void AfxEnableControlContainer() {}
inline BOOL AfxWinInit(HINSTANCE, HINSTANCE, LPTSTR, int) { return TRUE; }
inline BOOL AfxSocketInit(void* = 0) { return TRUE; }
// LoadString reads the .rc string table, which the native build has no equivalent
// of. Returning 0 leaves the caller's buffer untouched; textpose.cpp's
// LoadEmotionStrings depends on the string table, so the native emotion-rule path
// needs the strings supplied another way (the frozen textpose golden lists them).
inline int LoadString(HINSTANCE, UINT, LPTSTR buf, int) { if (buf) buf[0] = '\0'; return 0; }

// ---------------------------------------------------------------------------
// Late additions, appended rather than slotted in - four ordering bugs this session
// came from inserting a method whose return or parameter type is declared further
// down, and each broke every translation unit rather than one line.
// ---------------------------------------------------------------------------

// CCSPropertySheet is the project's property-sheet base (chicdial.h:62), stubbed
// alongside CCSDialog and CCSPropertyPage for the same reason: including the real
// header drags a large Win32 surface into every translation unit.
#define PSH_HASHELP         0x00000200
#define PSH_PROPTITLE       0x00000001
#define PSH_NOAPPLYNOW      0x00000080
typedef struct tagPROPSHEETHEADER {
    DWORD dwSize, dwFlags;
    HWND  hwndParent;
    HINSTANCE hInstance;
    LPCSTR pszCaption;
    UINT  nPages, nStartPage;
} PROPSHEETHEADER;

class CCSPropertySheet : public CPropertySheet {
public:
    // MFC exposes the raw PROPSHEETHEADER so callers can set flags; protsupp.cpp
    // sets PSH_HASHELP on it.
    PROPSHEETHEADER m_psh;
    CCSPropertySheet() { memset(&m_psh, 0, sizeof(m_psh)); }
    CCSPropertySheet(UINT, CWnd* = 0, UINT = 0) { memset(&m_psh, 0, sizeof(m_psh)); }
    CCSPropertySheet(LPCTSTR, CWnd* = 0, UINT = 0) { memset(&m_psh, 0, sizeof(m_psh)); }
};

// LV_FINDINFO for CListCtrl::FindItem.
typedef struct _LV_FINDINFO {
    UINT   flags;
    LPCSTR psz;
    LPARAM lParam;
    POINT  pt;
    UINT   vkDirection;
} LV_FINDINFO, LVFINDINFO;

#endif // NATIVE_SHIM_MFCUI_H
