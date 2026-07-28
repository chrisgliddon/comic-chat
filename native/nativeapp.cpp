// nativeapp.cpp - the native application object.
//
// Replaces the raw zeroed storage that native/nativeglue.cpp used to supply for
// theApp and cui. That stub was fine for the asset dumps, which never touch
// application state, but the corpus replay reads fonts, art directories and flags,
// so a real CChatApp is needed.
//
// ===========================================================================
// THIS IS A DOCUMENTED SUBSET OF CChatApp::CChatApp (chat.cpp:157-277), not a
// reimplementation and not the whole thing.
//
// The member initialisations below are transcribed faithfully - several are
// load-bearing for the harness (m_flags1, m_charSet, m_strDefaultArtDir) and a
// wrong value would change layout rather than merely disable something.
//
// DELIBERATELY OMITTED, with the reason for each:
//
//   * The rules and notifications wiring (m_dynaRules/m_dynaNotifs setter calls).
//     Those take function pointers into rules.cpp, actions.cpp and notif.cpp, none
//     of which compile natively yet, and the corpus replay drives ProcessLine
//     directly without going through the rules engine.
//   * The GUI font (GetStockObject(DEFAULT_GUI_FONT) -> MatchFont -> m_fontGui).
//     There are no Win32 stock objects here, and the frozen glyph table pins the
//     COMICS font, not the GUI one - so a GUI font would be unmeasurable anyway.
//     m_szGuiFaceName is left empty; InitHarness sets it explicitly.
//   * m_ImageList.Create - member-list icons, no part of a geometry dump.
//   * The service connector and notification thread handles.
//
// The omissions are an experiment with a referee, not an assumption: the Windows
// oracle links the REAL chat.cpp, so its goldens encode the real constructor. If
// the native corpus dump reproduces those goldens, the omitted parts demonstrably
// do not affect the dump. If it does not, the diff says which one to add. That is
// the whole reason for attempting a subset rather than making chat.cpp compile
// first - it converts a 2871-line lift into a measurement.
// ===========================================================================

#include "stdafx.h"
#include "chat.h"
#include "ui.h"
// TEXT_VIEW_BLANK_NEVER lives in the vendored textview.h, which chat.cpp gets via
// its own include chain.
#include "textview.h"
#include "rtfctrl.h"

#include <time.h>

// cui's constructor is inline in ui.h, so the definition alone suffices.
CUI cui;
CChatApp theApp;

CChatApp::CChatApp()
{
    // --- transcribed from chat.cpp:159-210 -------------------------------------
    m_bNoRefresh = FALSE;
    m_xFrame = m_yFrame = m_cxFrame = m_cyFrame = m_maxedFrame = 0;
    m_bComicView = FALSE;
    m_bShowMode = FALSE;
    m_bVIPMode = FALSE;
    m_bDisableMOTD = FALSE;
    m_textSpacing = TEXT_VIEW_BLANK_NEVER;
    m_bShowArrivals = TRUE;
    m_bAllowInvites = TRUE;
    m_bCfInitialized = FALSE;
    m_bCfHLInitialized = FALSE;
    m_iHostHighlight = HH_BOLD_HEADERS | HH_BOLD_MESSAGES;
    m_iGreetingType = 0;
    m_iShowBars = SB_TOOLBAR_ANY | SB_STATUSBAR;
    m_charSet = ANSI_CHARSET;
    m_bPlaySounds = TRUE;
    m_bNoMIDI = FALSE;
    m_bAcceptNMCalls = TRUE;
    m_bShowIdentity = TRUE;
    m_bListRegistered = FALSE;
    SetRectEmpty(&m_rectWhisper);
    SetRectEmpty(&m_rectNotifs);
    m_bSaveViewMode = TRUE;
    m_bAllowFileTX = TRUE;
    m_uFloodFlags = FLOOD_IGNORE;
    m_uFloodCount = 8;
    m_uFloodInterval = 8;
    m_iAutoPage = -1;
    // ~0 on every flag: F1_RTFCOMIC among them, which InitHarness relies on.
    m_flags1 = ~0;
    m_flags0 = 0;
    m_pmenuAdmin = NULL;
    m_iOnConnectAction = CA_JOINROOM;
    m_bLoadURL = FALSE;
    m_bIconMembers = TRUE;
    m_bPrompt = FALSE;
    m_bAcceptWhispers = TRUE;
    m_bEmbedded = FALSE;
    m_bLoginNotifsShown = FALSE;
    m_bAway = FALSE;
    m_bDoCB32 = FALSE;
    m_bDoTest = FALSE;
    m_szGuiFaceName[0] = '\0';
    m_lfGuiPitchAndFamily = 0;
    // "in case registry corrupted" in the original - and the native build has no
    // registry at all, so this is the only value it will ever have.
    m_strDefaultArtDir = "ComicArt";
    m_pNetRequestor = NULL;
    m_bAutoDownloadAvatars = FALSE;
    m_bAutoDownloadBackdrops = TRUE;

    // --- transcribed from chat.cpp:227-235 -------------------------------------
    // The scratch buffers. Kept because format.cpp writes through them; the size
    // expression is copied rather than simplified so it tracks the same constants.
    SHORT nBufferSize = (SHORT)max(g_nDefaultIOBuff + 1,
                                   (MAX_FORMATTINGPERBYTE + 1) * MAX_INPUTLEN);
    m_szBuffer = new CHAR[nBufferSize];
    m_wszBuffer = new WCHAR[nBufferSize];
    *m_wszBuffer = *m_szBuffer = '\0';
    m_nBufferSize = nBufferSize;

    m_nMyIdentLength = 0;
    m_pWndActiveDialog = NULL;
    m_pExitingDoc = NULL;
    m_pWndHiddenInThread = NULL;
    m_pbCoolBarState = NULL;
    m_hNotificationThread = NULL;
    m_hShutdownEvent = NULL;

    // chat.cpp:264 does srand((unsigned) time(NULL)) here, for the rules engine's
    // random actions. Reproduced for faithfulness, but note the hazard: it makes
    // the RNG time-dependent from construction until something reseeds it. The
    // oracle is safe because OracleSeedActivate calls srand(seed) before any
    // sequence is consumed - a global constructor runs before main, and nothing
    // between the two draws. Any native code that consumes rand() during startup
    // would break determinism, so keep that ordering.
    srand((unsigned) time(NULL));
}

CChatApp::~CChatApp()
{
    // The original also tears down the admin menu and the rules/notification
    // daemons. Nothing here allocates a menu, and the daemons were never wired up,
    // so the buffers are all there is to release. Process-lifetime object in a
    // one-shot dump either way.
    delete[] m_szBuffer;
    delete[] m_wszBuffer;
    m_szBuffer = NULL;
    m_wszBuffer = NULL;
}


// ===========================================================================
// CChatApp's own virtual overrides, and the constructors of its by-value members.
//
// These are needed for a REASON worth naming: CChatApp holds CCDynaRules,
// CCRulesData, CCDynaNotifs, CCDelayedRules, CChatServiceConnector and CDosKey as
// members by value, so their constructors run whether or not the rules engine is
// ever driven. They live in rules.cpp, notif.cpp and chatsrv.cpp, none of which
// compile natively.
//
// The constructors are NO-OPS rather than aborts - unlike everything else in this
// port's scaffolding - because they must succeed for theApp to exist at all. Their
// own member subobjects (the CPtrArrays and CStrings inside) still get
// default-constructed by the compiler, so each object is in a consistent empty
// state rather than uninitialised memory.
//
// Their METHODS are not defined here. Anything that actually drives the rules or
// notification engines will fail to link, which is the correct outcome: it names
// the subsystem being pulled in rather than silently doing nothing.
//
// CDosKey is the exception - doskey.cpp compiles, so it is LINKED rather than
// stubbed. Its ctor/dtor appear above only because the link line for the dump
// binaries did not include it; verify.sh now does.
// ===========================================================================

#define NATIVE_UNLINKED_APP(what) \
    do { \
        fprintf(stderr, "native: %s reached in a build with no windows (%s:%d)\n", \
                (what), __FILE__, __LINE__); \
        abort(); \
    } while (0)

// --- CChatApp virtuals. The dumps never run a message loop. ---
BOOL CChatApp::InitInstance()
{
    // Deliberately NOT the real InitInstance: that registers document templates,
    // creates the main frame and connects to a server. The dump drivers do their own
    // narrow setup instead (see InitHarness in the oracle harness).
    return TRUE;
}
int CChatApp::ExitInstance() { return 0; }
BOOL CChatApp::PreTranslateMessage(MSG*) { return FALSE; }
BOOL CChatApp::OnIdle(LONG) { return FALSE; }
int CChatApp::DoMessageBox(LPCSTR, UINT, UINT) { return 0; }

// CRtfCtrl is a rich-edit control CChatApp holds by value. Stubbed rather than
// compiling rtfctrl.cpp: that file is UI (owner-draw handlers, IME idle messages)
// and nothing in a geometry dump touches it, so compiling it would mean shimming a
// control's message map for two symbols.
CRtfCtrl::CRtfCtrl() {}
CRtfCtrl::~CRtfCtrl() {}
// Its virtuals, needed for the vtable. Abort rather than no-op: reaching one means
// UI code is running in a build that has no windows, which is worth a stack trace.
void CRtfCtrl::OnLButtonDown(UINT, CPoint) { NATIVE_UNLINKED_APP("CRtfCtrl::OnLButtonDown"); }
BOOL CRtfCtrl::PreCreateWindow(CREATESTRUCT&) { NATIVE_UNLINKED_APP("CRtfCtrl::PreCreateWindow"); }
BOOL CRtfCtrl::OnCmdMsg(UINT, int, void*, AFX_CMDHANDLERINFO*) { NATIVE_UNLINKED_APP("CRtfCtrl::OnCmdMsg"); }

// --- Rules / notifications / service-connector member constructors ---
CCRulesData::CCRulesData() {}
CCRulesData::~CCRulesData() {}
CCDynaRules::CCDynaRules() {}
CCDynaRules::~CCDynaRules() {}
CCDelayedRules::CCDelayedRules() {}
CCDelayedRules::~CCDelayedRules() {}
CCDynaNotifs::CCDynaNotifs() {}
CCDynaNotifs::~CCDynaNotifs() {}
CChatServiceConnector::CChatServiceConnector() {}
CChatServiceConnector::~CChatServiceConnector() {}

// CCItemPtrArray::FreeRemoveAll lives in rules.cpp. Empty for the same reason as
// FreeList below: the arrays were never populated.
void CCItemPtrArray::FreeRemoveAll() {}

// CListObject::FreeList is called from those destructors' base. Empty: nothing was
// ever added to the lists, because nothing constructed them for real.
void CListObject::FreeList() {}

// --- status reporting ----------------------------------------------------------------
// The engine narrates its own progress through the status bar: connecting, registering,
// MOTD, channel joined, errors. On Windows this writes into a CStatusBar pane.
//
// Printed rather than discarded, because during a connection these ARE the diagnostics -
// "reached a stub" tells you nothing about why an IRC registration was rejected, whereas the
// engine's own status text usually says exactly. The AppKit shell shows the same strings in
// its window title.
void CChatApp::SetStatusPaneString(int pane, const char* text) {
    if (text && *text) fprintf(stderr, "[status %d] %s\n", pane, text);
}

// CompleteConnection - called by the engine once the socket is up.
//
// The Windows implementation (chat.cpp:2587) only does bookkeeping for m_SrvConnector: it
// copies the name of the server that connector chose, and marks it as last-accessed. The
// native front end connects DIRECTLY (see NativeSessionConnect) precisely because the
// connector drives itself through AfxGetMainWnd()->SendMessage and SetTimer, so there is no
// connector state here and calling the real version would dereference a NULL
// GetConnectingServer().
//
// What it must still do is record which server is connected, because the rules engine and the
// status line read it back. The session stores that name via NativeSetMyServer, so this copies
// it into the app object where the engine expects it.
extern const char* GetMyServer();
void CChatApp::CompleteConnection() {
    const char* s = GetMyServer();
    m_strConnectedServer = (s && *s) ? s : "";
    m_strConnectedService = m_strConnectedServer;
}
