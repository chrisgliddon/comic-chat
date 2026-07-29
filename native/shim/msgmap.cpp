// msgmap.cpp - the message dispatcher, and the base maps that root the chain.
//
// This is the other half of msgmap.h: walk a class's map chain, find the entry for a
// message, restore the handler's real signature and call it. Once this exists, every
// afx_msg handler already in the tree becomes reachable - CBodyCam's emotion wheel,
// CChatDoc's whole command surface, CPageView's mouse - without any of them being touched.
//
// Nothing here is reachable from the oracle harness: no golden replays a message.

#include "stdafx.h"
#include "render.h"     // NativeWndPaint's declaration, and ApplicationServices

// --- painting -------------------------------------------------------------
// A window's client coordinates are device pixels with the origin at the TOP-left and y
// increasing DOWNWARD. Core Graphics puts the origin at the bottom-left with y increasing
// upward, so the CTM is flipped once here and every coordinate the engine computes lands
// correctly without any drawing code knowing about it.
//
// The rest of the CDC layer needs no change for this, which is worth stating because it is
// not obvious: cgdraw and cgblit express rectangles as CGRectMake(x, y + h, w, -h), and that
// form is correct under BOTH conventions. In engine page space y and h are negative, in a
// window they are positive, and either way the origin lands on the lower edge with the
// height running back toward the upper one. Text is the one exception, and takes an explicit
// yDown flag.
void NativeWndPaint(CWnd* wnd, CGContextRef ctx, int widthPx, int heightPx) {
    if (!wnd || !ctx) return;
    wnd->NativeSetClientSize(widthPx, heightPx);
    wnd->m_paintCtx = (void*)ctx;

    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, (CGFloat)heightPx);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    wnd->SendMessage(WM_PAINT);
    CGContextRestoreGState(ctx);

    wnd->m_paintCtx = 0;
}

// The DCs an OnPaint constructs. They pick the context up from the window, which is what
// makes an unmodified `CPaintDC dc(this)` reach the screen.
//
// A CPaintDC with no context is a BUG: it means a WM_PAINT was dispatched without the host
// binding a surface, and since every CDC entry point checks m_cgCtx and returns quietly, the
// result is a blank pane with every drawing call apparently succeeding. Reported once.
CPaintDC::CPaintDC(CWnd* pWnd) {
    if (pWnd) m_cgCtx = pWnd->m_paintCtx;
    m_yDown = TRUE;             // a window's client coordinates; NativeWndPaint flipped the CTM
    if (!m_cgCtx) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            fprintf(stderr, "native: CPaintDC(%p) has no paint context - everything drawn "
                            "through it will be discarded.\n", (void*)pWnd);
        }
    }
}

// A CClientDC OUTSIDE a paint is not a bug - it is GDI's way of drawing right now, without
// waiting for WM_PAINT. CBodyCam::RefreshBody uses it to update the character after an
// emotion changes. There is no equivalent on a retained-backing-store system, so the honest
// translation is to INVALIDATE and let the next WM_PAINT redraw: the same pixels arrive, one
// frame later. Silent, because it is the expected path rather than a fault.
CClientDC::CClientDC(CWnd* pWnd) {
    m_yDown = TRUE;
    if (!pWnd) return;
    m_cgCtx = pWnd->m_paintCtx;
    if (!m_cgCtx) pWnd->InvalidateRect();
}

// --- the root of every map chain -------------------------------------------
// CCmdTarget's map is empty and its base pointer is NULL, which is what terminates the
// walk. Written out by hand because BEGIN_MESSAGE_MAP needs a base class to chain to and
// CCmdTarget has none.
static const AFX_MSGMAP_ENTRY _cmdTargetEntries[] = {
    { 0, 0, 0, 0, AfxSig_end, (AFX_PMSG)0 }
};
static const AFX_MSGMAP _cmdTargetMap = { 0, &_cmdTargetEntries[0] };

const AFX_MSGMAP* CCmdTarget::GetThisMessageMap() { return &_cmdTargetMap; }
const AFX_MSGMAP* CCmdTarget::GetMessageMap() const { return GetThisMessageMap(); }

// The shim's own UI classes. Their maps are empty - they exist only so a derived engine
// class has something to chain to, and so an unhandled message stops cleanly rather than
// walking off the end of a NULL pointer.
BEGIN_MESSAGE_MAP(CWnd, CCmdTarget)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CView, CWnd)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CScrollView, CView)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CFrameWnd, CWnd)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CDocument, CCmdTarget)
END_MESSAGE_MAP()

// --- focus -----------------------------------------------------------------
// One focused window at a time, so ::GetFocus() == m_hWnd comparisons in the engine mean
// something. CBodyCam::OnPaint draws a focus rect on the strength of exactly that test.
static CWnd* g_focusWnd = 0;

CWnd* CWnd::GetFocus() { return g_focusWnd; }

void NativeSetFocusWnd(CWnd* p) {
    if (g_focusWnd == p) return;
    CWnd* old = g_focusWnd;
    g_focusWnd = p;
    if (old) { old->m_hasFocus = false; old->SendMessage(WM_KILLFOCUS, (WPARAM)(p ? p->m_hWnd : 0)); }
    if (p)   { p->m_hasFocus = true;    p->SendMessage(WM_SETFOCUS,  (WPARAM)(old ? old->m_hWnd : 0)); }
}

// Declared in the shim's win32 layer; routed here now that focus is real.
HWND NativeGetFocusHwnd() { return g_focusWnd ? g_focusWnd->GetSafeHwnd() : 0; }

// --- invalidation ----------------------------------------------------------
// The host installs this so InvalidateRect can reach -[NSView setNeedsDisplay:]. Kept as a
// function pointer rather than an AppKit call so this file stays out of Objective-C and the
// headless drivers link without AppKit.
static void (*g_invalidate)(void* hostView) = 0;

void NativeSetInvalidateHook(void (*fn)(void*)) { g_invalidate = fn; }

void CWnd::InvalidateRect(const RECT*, BOOL) {
    if (g_invalidate && m_hostView) g_invalidate(m_hostView);
}

// --- dispatch --------------------------------------------------------------

// The handler signatures, for casting AFX_PMSG back before the call. See msgmap.h on why
// this is sound for the single-inheritance hierarchy these classes form.
typedef void    (CWnd::*PFN_vv)();
typedef void    (CWnd::*PFN_vw)(UINT);
typedef void    (CWnd::*PFN_vwP)(UINT, CPoint);
typedef void    (CWnd::*PFN_vwww)(UINT, UINT, UINT);
typedef void    (CWnd::*PFN_vwii)(UINT, int, int);
typedef void    (CWnd::*PFN_vp)(CWnd*);
typedef void    (CWnd::*PFN_vpP)(CWnd*, CPoint);
typedef void    (CWnd::*PFN_vwp)(UINT, CWnd*);
typedef void    (CWnd::*PFN_vwwh)(UINT, UINT, HMENU);
typedef void    (CWnd::*PFN_vwwx)(UINT, UINT, CScrollBar*);
typedef void    (CWnd::*PFN_vbw)(BOOL, UINT);
typedef UINT    (CWnd::*PFN_wv)();
typedef int     (CWnd::*PFN_iC)(LPCREATESTRUCT);
typedef BOOL    (CWnd::*PFN_bD)(CDC*);
typedef BOOL    (CWnd::*PFN_bwsP)(UINT, short, CPoint);
typedef LRESULT (CWnd::*PFN_lwl)(WPARAM, LPARAM);
typedef void    (CCmdTarget::*PFN_vU)(CCmdUI*);
typedef void    (CCmdTarget::*PFN_vv_ct)();
typedef void    (CCmdTarget::*PFN_vw_ct)(UINT);

// LOWORD/HIWORD of an LPARAM as a point, the way Win32 packs mouse coordinates. The y is
// SIGNED - a drag can leave the window and MFC's handlers rely on seeing negatives.
static CPoint PointFromLParam(LPARAM lParam) {
    return CPoint((short)(lParam & 0xFFFF), (short)((lParam >> 16) & 0xFFFF));
}

BOOL CWnd::OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
    LRESULT result = 0;

    for (const AFX_MSGMAP* map = GetMessageMap(); map; ) {
        for (const AFX_MSGMAP_ENTRY* e = map->lpEntries; e->nSig != AfxSig_end; e++) {
            if (e->nMessage != message) continue;
            // Command and update-UI entries carry nMessage 0 and are routed by OnCmdMsg,
            // not from here. A WM_COMMAND with a notification code is a control
            // notification and needs its id matched.
            if (message == WM_COMMAND && e->nCode != 0) {
                if (e->nID != (UINT)LOWORD(wParam)) continue;
                if (e->nCode != (UINT)HIWORD(wParam)) continue;
            }
            if (message == WM_NOTIFY) {
                NMHDR* pnmh = (NMHDR*)lParam;
                if (!pnmh) continue;
                if (e->nID < pnmh->idFrom || e->nLastID > pnmh->idFrom) {
                    if (e->nID != (UINT)pnmh->idFrom) continue;
                }
                if (e->nCode != (UINT)(int)pnmh->code) continue;
            }

            switch (e->nSig) {
                case AfxSig_vv:   (this->*(PFN_vv)e->pfn)(); break;
                case AfxSig_vw:   (this->*(PFN_vw)e->pfn)((UINT)wParam); break;
                case AfxSig_vwP:
                    (this->*(PFN_vwP)e->pfn)((UINT)wParam, PointFromLParam(lParam));
                    break;
                case AfxSig_vwww:
                    // WM_KEYDOWN / WM_CHAR: char code, repeat count, flags.
                    (this->*(PFN_vwww)e->pfn)((UINT)wParam, (UINT)(lParam & 0xFFFF),
                                              (UINT)((lParam >> 16) & 0xFFFF));
                    break;
                case AfxSig_vwii:
                    (this->*(PFN_vwii)e->pfn)((UINT)wParam, (int)(short)(lParam & 0xFFFF),
                                              (int)(short)((lParam >> 16) & 0xFFFF));
                    break;
                case AfxSig_vp:
                    (this->*(PFN_vp)e->pfn)(CWnd::FromHandle((HWND)wParam));
                    break;
                case AfxSig_vpP:
                    // WM_CONTEXTMENU: wParam is the window, lParam the SCREEN point.
                    (this->*(PFN_vpP)e->pfn)(CWnd::FromHandle((HWND)wParam),
                                             PointFromLParam(lParam));
                    break;
                case AfxSig_vwp:
                    (this->*(PFN_vwp)e->pfn)((UINT)wParam, CWnd::FromHandle((HWND)lParam));
                    break;
                case AfxSig_vwwh:
                    (this->*(PFN_vwwh)e->pfn)((UINT)LOWORD(wParam), (UINT)HIWORD(wParam),
                                              (HMENU)lParam);
                    break;
                case AfxSig_vwwx:
                    (this->*(PFN_vwwx)e->pfn)((UINT)LOWORD(wParam), (UINT)HIWORD(wParam), 0);
                    break;
                case AfxSig_vbw:
                    (this->*(PFN_vbw)e->pfn)((BOOL)wParam, (UINT)lParam);
                    break;
                case AfxSig_wv:   result = (LRESULT)(this->*(PFN_wv)e->pfn)(); break;
                case AfxSig_iC:
                    result = (this->*(PFN_iC)e->pfn)((LPCREATESTRUCT)lParam);
                    break;
                case AfxSig_bD:
                    result = (this->*(PFN_bD)e->pfn)((CDC*)wParam);
                    break;
                case AfxSig_bwsP:
                    // WM_MOUSEWHEEL: keys in the low word, delta in the high word (signed).
                    result = (this->*(PFN_bwsP)e->pfn)((UINT)LOWORD(wParam),
                                                       (short)HIWORD(wParam),
                                                       PointFromLParam(lParam));
                    break;
                case AfxSig_lwl:
                    result = (this->*(PFN_lwl)e->pfn)(wParam, lParam);
                    break;
                case AfxSig_unsupported:
                    // A real entry whose shape is not dispatched yet. Reported as
                    // unhandled so the default path still runs.
                    continue;
                default:
                    continue;
            }
            if (pResult) *pResult = result;
            return TRUE;
        }
        map = map->pfnGetBaseMap ? map->pfnGetBaseMap() : 0;
    }
    return FALSE;
}

LRESULT CWnd::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    // A WM_COMMAND with no notification code is a menu pick or an accelerator, and goes
    // through command routing so ON_COMMAND / ON_COMMAND_RANGE entries are found. This is
    // the path that makes the original menus work.
    if (message == WM_COMMAND && HIWORD(wParam) == 0) {
        if (OnCmdMsg((UINT)LOWORD(wParam), CN_COMMAND, 0, 0)) return 0;
    }
    LRESULT result = 0;
    if (OnWndMsg(message, wParam, lParam, &result)) return result;
    return DefWindowProc(message, wParam, lParam);
}

BOOL CCmdTarget::OnCmdMsg(UINT nID, int nCode, void* pExtra,
                          struct AFX_CMDHANDLERINFO* /*pHandlerInfo*/) {
    for (const AFX_MSGMAP* map = GetMessageMap(); map; ) {
        for (const AFX_MSGMAP_ENTRY* e = map->lpEntries; e->nSig != AfxSig_end; e++) {
            if (e->nMessage != 0) continue;                 // not a command entry
            if (e->nCode != (UINT)nCode) continue;
            if (nID < e->nID || nID > e->nLastID) continue;

            if (nCode == CN_COMMAND) {
                // A range handler is told which id fired; a single handler takes nothing.
                if (e->nSig == AfxSig_vw) (this->*(PFN_vw_ct)e->pfn)(nID);
                else                      (this->*(PFN_vv_ct)e->pfn)();
                return TRUE;
            }
            if (nCode == (int)CN_UPDATE_COMMAND_UI) {
                CCmdUI* pUI = (CCmdUI*)pExtra;
                if (!pUI) return FALSE;
                pUI->m_nID = nID;
                (this->*(PFN_vU)e->pfn)(pUI);
                return TRUE;
            }
        }
        map = map->pfnGetBaseMap ? map->pfnGetBaseMap() : 0;
    }
    return FALSE;
}
