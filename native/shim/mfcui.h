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

// --- the class hierarchy ---------------------------------------------------
class CCmdTarget : public CObject {
public:
    virtual ~CCmdTarget() {}
    void EnableAutomation() {}
};

class CWnd : public CCmdTarget {
public:
    HWND m_hWnd;
    CWnd() : m_hWnd(0) {}
    virtual ~CWnd() {}
    static CWnd* FromHandle(HWND) { return 0; }
    HWND GetSafeHwnd() const { return m_hWnd; }
    BOOL IsWindow() const { return FALSE; }
    void GetClientRect(RECT* r) const { if (r) { r->left = r->top = r->right = r->bottom = 0; } }
    void GetWindowRect(RECT* r) const { if (r) { r->left = r->top = r->right = r->bottom = 0; } }
    void InvalidateRect(const RECT*, BOOL = TRUE) {}
    void Invalidate(BOOL = TRUE) {}
    void UpdateWindow() {}
    BOOL ShowWindow(int) { return TRUE; }
    void SetWindowText(LPCTSTR) {}
    int GetWindowText(LPTSTR, int) const { return 0; }
    CWnd* GetParent() const { return 0; }
    CWnd* GetDlgItem(int) const { return 0; }
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

class CFrameWnd : public CWnd {};
class CMDIFrameWnd : public CFrameWnd {};
class CMDIChildWnd : public CFrameWnd {};
class CMiniFrameWnd : public CFrameWnd {};
class CSplitterWnd : public CWnd {};
class CControlBar : public CWnd {};
class CToolBar : public CControlBar {};
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
    void EndDialog(int) {}
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
class CPropertySheet : public CWnd {
public:
    CPropertySheet() {}
    CPropertySheet(LPCTSTR, CWnd* = 0, UINT = 0) {}
    void AddPage(CPropertyPage*) {}
    virtual int DoModal() { return 0; }
};

class CView : public CWnd {
public:
    virtual void OnDraw(CDC*) {}
    class CDocument* GetDocument() const { return 0; }
};
class CScrollView : public CView {};

class CDocument : public CCmdTarget {
public:
    virtual ~CDocument() {}
    void SetModifiedFlag(BOOL = TRUE) {}
    void UpdateAllViews(CView*, LONG = 0, CObject* = 0) {}
    CString GetPathName() const { return CString(); }
    void SetTitle(LPCTSTR) {}
};
class COleDocument : public CDocument {};
class COleServerDoc : public COleDocument {};
class CDocObjectServerDoc : public COleServerDoc {};
class CDocObjectServerItem : public CCmdTarget {};
class COleServerItem : public CCmdTarget {};

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
};
class CListCtrl : public CWnd {
public:
    int GetItemCount() const { return 0; }
    BOOL DeleteAllItems() { return TRUE; }
    int InsertItem(const void*) { return 0; }
    DWORD GetItemData(int) const { return 0; }
    BOOL SetItemData(int, DWORD) { return TRUE; }
};
class CTreeCtrl : public CWnd {};
class CTabCtrl : public CWnd {};
class CProgressCtrl : public CWnd {};
class CSliderCtrl : public CWnd {};
class CRichEditCtrl : public CWnd {
public:
    void SetSel(long, long) {}
    void ReplaceSel(LPCTSTR) {}
    long GetTextLength() const { return 0; }
};
class CFileDialog : public CDialog {
public:
    CFileDialog(BOOL, LPCTSTR = 0, LPCTSTR = 0, DWORD = 0, LPCTSTR = 0, CWnd* = 0) {}
    CString GetPathName() const { return CString(); }
    CString GetFileName() const { return CString(); }
};
class CFontDialog : public CDialog {
public:
    CFontDialog() {}
    CFontDialog(LOGFONT*, DWORD = 0, CDC* = 0, CWnd* = 0) {}
};
class CMenu : public CObject {
public:
    CMenu* GetSubMenu(int) { return 0; }
    BOOL AppendMenu(UINT, UINT = 0, LPCTSTR = 0) { return TRUE; }
    UINT CheckMenuItem(UINT, UINT) { return 0; }
    UINT EnableMenuItem(UINT, UINT) { return 0; }
};

// --- sockets ---------------------------------------------------------------
// The engine's own chatsock replaces most of this; CAsyncSocket appears only as a
// base class in a header. Real networking will use BSD sockets directly.
class CAsyncSocket : public CObject {
public:
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
};

// --- free functions --------------------------------------------------------
inline CWinApp* AfxGetApp() { return 0; }
inline CWnd* AfxGetMainWnd() { return 0; }
inline int AfxMessageBox(LPCTSTR, UINT = 0, UINT = 0) { return 0; }
inline BOOL AfxOleInit() { return TRUE; }
inline void AfxEnableControlContainer() {}
inline BOOL AfxWinInit(HINSTANCE, HINSTANCE, LPTSTR, int) { return TRUE; }
inline BOOL AfxSocketInit(void* = 0) { return TRUE; }
// LoadString reads the .rc string table, which the native build has no equivalent
// of. Returning 0 leaves the caller's buffer untouched; textpose.cpp's
// LoadEmotionStrings depends on the string table, so the native emotion-rule path
// needs the strings supplied another way (the frozen textpose golden lists them).
inline int LoadString(HINSTANCE, UINT, LPTSTR buf, int) { if (buf) buf[0] = '\0'; return 0; }

#endif // NATIVE_SHIM_MFCUI_H
