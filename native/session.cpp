// session.cpp - see session.h.

#include "stdafx.h"
#include "session.h"

#include "chat.h"
#include "userinfo.h"
#include "chatprot.h"
#include "binddoc.h"
#include "chatdoc.h"
#include "ui.h"
#include "vector2d.h"
#include "traj.h"
#include "spline.h"
#include "bbox.h"
#include "pe.h"
#include "dib.h"
#include "avbfile.h"
#include "avatar.h"
#include "balloon.h"
class CPageView;
#include "backdrop.h"
#include "panel.h"
#include "glyphtable.h"
#include "stringtable.h"
#include "bodycam.h"
#include "ircproto.h"
#include "ircsock.h"
#include "defines.h"

#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <string.h>

extern CChatApp theApp;
extern CUI cui;

// Declared by the engine but defined in files the native build does not compile.
void InitializeBackDrops();
void InitializeEmotionRules();
void InitializeAvatars();
void LoadEmotionStrings();
CAvatarX* LoadAvatar(const char* avName);
int PointsToTwips(int pts);
BOOL ArtDirsOK();

namespace {

// A CChatDoc with the view hooks disarmed. CChatDoc's own methods call UpdateViewsX and
// RefreshPanelN, which expect an MFC CView; the AppKit front end redraws on its own schedule
// instead, so m_view stays NULL and theApp.m_bNoRefresh suppresses the rest.
class CNativeChatDoc : public CChatDoc {
public:
    CNativeChatDoc() : CChatDoc() {}
};

CNativeChatDoc* g_doc = 0;
CPtrList        g_userInfos;      // keeps the synthesized CUserInfos alive
bool            g_started = false;

} // namespace

bool NativeSessionStart(const char* treeDir) {
    if (g_started) return true;
    if (!treeDir) treeDir = ".";

    // The frozen data files. Loaded up front so a missing one is reported here rather than
    // as an abort from the middle of layout. The string table is not optional: textpose.cpp
    // reads its emotion-detection RULES out of it, so without it every pose decision differs.
    if (!GlyphTableLoad(0)) {
        fprintf(stderr, "session: no glyph table - text cannot be measured.\n");
        return false;
    }
    if (!StringTableLoad(0)) {
        fprintf(stderr, "session: no string table - there would be no emotion rules.\n");
        return false;
    }

    AfxWinInit(GetModuleHandle(NULL), NULL, ::GetCommandLine(), SW_HIDE);

    // The measurement DC: desktop client DC in MM_TWIPS, as pageview.cpp:994 sets up.
    if (!cui.m_pvClientDC) {
        CClientDC* dc = new CClientDC(CWnd::FromHandle(::GetDesktopWindow()));
        dc->SetMapMode(MM_TWIPS);
        cui.m_pvClientDC = dc;
    }

    // Panel dimensions BEFORE SetFonts: UpdateTitleFonts scales the title and shout fonts
    // by m_unitWidth, so setting them afterwards leaves those fonts sized for the wrong
    // panel and every title mispositioned.
    CUnitPanelPage::SetUnitPanelsPerRow(2);
    CUnitPanelPage::SetUnitPanelWidth(4860);
    CUnitPanelPage::SetUnitPanelHeight(4860);

    int fontHeight = PointsToTwips(12);           // IDS_DFLT_COMICSPNTSIZE
    memset(&theApp.m_comicsFont, 0, sizeof(LOGFONT));
    theApp.m_comicsFont.lfHeight = fontHeight;
    theApp.m_comicsFont.lfWeight = FW_NORMAL;
    theApp.m_comicsFont.lfCharSet = ANSI_CHARSET;
    theApp.m_comicsFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    theApp.m_comicsFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    theApp.m_comicsFont.lfQuality = DEFAULT_QUALITY;
    theApp.m_comicsFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    strcpy(theApp.m_comicsFont.lfFaceName, "Comic Sans MS");
    theApp.m_iFontHeightBalloon = fontHeight;
    theApp.m_comicsColor = RGB(0, 0, 0);
    theApp.m_textColor   = RGB(0, 0, 0);
    theApp.m_charSet     = ANSI_CHARSET;
    theApp.m_bComicView  = TRUE;
    // The engine would otherwise call into an MFC view on every line.
    theApp.m_bNoRefresh  = TRUE;
    theApp.m_flags1      = (DWORD)~0;
    theApp.m_bIconMembers = TRUE;
    strcpy(theApp.m_szGuiFaceName, "Comic Sans MS");

    CUnitPanelPage::SetFonts(theApp.m_comicsFont, theApp.m_comicsColor);

    // m_strDefaultArtDir must be a RELATIVE name: SetArtDir (protsupp.cpp:164) builds the
    // avatar directory as m_strBaseDir + "\\" + m_strDefaultArtDir itself, so a full path
    // here gets double-joined and every avatar load fails.
    theApp.m_strBaseDir       = treeDir;
    theApp.m_strDefaultArtDir = "ComicArt";
    theApp.m_strBackDropDir   = CString(treeDir) + "\\ComicArt";
    theApp.m_strAvatarDir     = theApp.m_strBackDropDir;

    // m_bFoundArt is LOAD-BEARING, not informational. AdjustViewMode (protsupp.cpp:152) reads
    //     if (g_iViewMode == VM_TEXT || !theApp.m_bFoundArt) doc->OnViewText();
    // and the room-entry path calls AdjustViewMode, so leaving it FALSE sent the engine into
    // the TEXT view on every channel join - which then reached into CChatView and aborted.
    // Windows sets it inside SetArtDir; the native path assigns the directories itself (to
    // avoid SetArtDir's early-out when they are unchanged), so it must set this too.
    theApp.m_bFoundArt = ArtDirsOK();
    if (!theApp.m_bFoundArt)
        fprintf(stderr, "session: ComicArt not found under '%s' - characters will be missing\n",
                treeDir);

    InitializeBackDrops();
    LoadEmotionStrings();
    InitializeEmotionRules();
    InitializeAvatars();

    g_doc = new CNativeChatDoc();
    cui.m_pvChatDoc = g_doc;      // GetChatDoc() finds it here
    g_doc->m_view = NULL;         // see CNativeChatDoc
    g_doc->m_bComicView = TRUE;
    g_doc->SetComicsTitle("Comic Chat");
    // The BODY CAM. GetBodyCam() (chatdoc.cpp:2028) returns pDoc->m_bodyCam, and RefreshBodyCam
    // dereferences it WITHOUT a null check (bodycam.cpp:868) - so an unset one crashed the
    // moment the engine set the local avatar, which InitHistory does on every channel join.
    //
    // A real instance rather than a guard: CBodyCam's constructor only initialises members and
    // DPI-scales the emotion-wheel metrics, and its window creation is behind a VERIFY that
    // fails harmlessly with no window server. It is also the object the self-view and emotion
    // wheel will be driven from, so it belongs here anyway.
    g_doc->m_bodyCam = new CBodyCam;

    g_doc->InitMyDocument();

    // m_enterInfos entry 0 is g_enterInfo, which is what GetRoomInfoFromName falls back to
    // when a room is not in the list (chat.cpp:1870 indexes entry 0 and overwrites its
    // channel). Seeding it is what makes that fallback safe rather than an empty-array index.
    if (theApp.m_enterInfos.GetSize() == 0) theApp.m_enterInfos.Add(&g_enterInfo);

    // TWO protocol objects, and they must stay distinct - this is the engine's own structure,
    // not an accident:
    //
    //   cui.m_pvIrcProto  the CONNECTION-level proto. CommunicationInits creates it once with
    //                     NewDefaultProto(NULL), and GetIrcProto() returns it. Connection
    //                     status and every outgoing query go through it.
    //   doc->m_proto      the ROOM-level proto, replaced on every join: the JOIN handler calls
    //                     bProcessAddChannel(channel, NewDefaultProto(NULL), ...) and that
    //                     function does `delete doc->m_proto` before installing the new one
    //                     (protsupp.cpp:4321).
    //
    // Aliasing them - which looked like a simplification, since CChatDoc's constructor already
    // makes a proto - meant the join DELETED the object GetIrcProto() returns. The next query
    // then read m_pSock through a dangling pointer, got NULL, and crashed. So the
    // simplification was actively wrong, and the duplication is load-bearing.
    if (!cui.m_pvIrcProto) cui.m_pvIrcProto = NewDefaultProto(NULL);
    currentRoom = g_doc->m_proto;

    // The engine looks up "who am I" through g_puiSelf when deciding whether a message is
    // from the local user. CChatDoc::LoadDocData sets it from m_puiSelf on Windows.
    g_doc->LoadDocData();

    g_started = true;
    return true;
}

unsigned short NativeSessionAddSpeaker(const char* avatarName, const char* nickname) {
    if (!g_started || !avatarName) return 0;
    CAvatarX* av = LoadAvatar(avatarName);
    if (!av) {
        fprintf(stderr, "session: could not load avatar '%s'\n", avatarName);
        return 0;
    }
    // Every avatar needs a CUserInfo: panel ordering (EvalPair/AddTalkTos) dereferences
    // av->m_userInfo->m_udi.m_talkTos, so an avatar without one crashes during layout rather
    // than merely looking wrong.
    CUserInfo* pui = new CUserInfo();
    pui->SetAvatarID(av->m_avatarID);
    pui->SetName(nickname && *nickname ? nickname : avatarName);
    av->m_userInfo = pui;
    g_userInfos.AddTail(pui);
    return av->m_avatarID;
}

void NativeSessionSetSelf(unsigned short avatarID) {
    if (g_doc) g_doc->m_myAvatarID = avatarID;
}

void NativeSessionSay(unsigned short avatarID, const char* text) {
    if (!g_doc || !text || !*text) return;
    // BM_SAY, and bbCooked TRUE: the text is already the user's literal words, so the
    // engine should run its emotion detection over it rather than treat it as pre-parsed.
    g_doc->ProcessLine(avatarID, text, (USHORT)1, (BYTE)TRUE, NULL);
}

CPage* NativeSessionCurrentPage() {
    if (!g_doc || g_doc->m_pages.IsEmpty()) return 0;
    // AddNewPage does AddHead and AddLine uses GetTail, so the page being composed is the
    // TAIL - the head is the oldest. Getting this backwards shows an empty page.
    return (CPage*)g_doc->m_pages.GetTail();
}

// --- IRC ---------------------------------------------------------------------------

extern CIrcSocket serverConn;
// GetIrcProto() is a MACRO in ui.h (cui.GetIrcProtoPv() cast to CIrcProto*), not a function -
// declaring it as one collides with the macro expansion.
void SetMyName(const char* szCharName);
void NativeSetMyServer(const char* server);
void SetMyNameNick(const char* szNickname);
const char* GetMyName();

bool NativeSessionConnect(const char* server, int port, const char* nickname,
                          const char* channel) {
    if (!g_started || !server || !*server) return false;
    if (port <= 0) port = 6667;

    // The engine reads the nick back out of its own settings when it builds the NICK/USER
    // registration in CIrcSocket::HrIrcLogin, so it has to be set here rather than passed.
    if (nickname && *nickname) {
        SetMyName(nickname);
        SetMyNameNick(nickname);
    }

    // The room to join on login. CIrcProto::OnLogin does ChatJoinChannel(g_enterInfo) when
    // m_iOnConnectAction is CA_JOINROOM, so filling this in is what makes the engine join by
    // itself - and leaving it empty is what made it send a bare "JOIN" that the server
    // rejected with 461.
    if (channel && *channel) {
        g_enterInfo.m_strChannel = channel;
        theApp.m_iOnConnectAction = CA_JOINROOM;

        // The channel is NOT set on the proto here. bProcessAddChannel installs a fresh room
        // proto on join and sets its channel itself, which is what LookupDoc then matches -
        // setting it on the doc's initial proto only mattered while the two protos were
        // wrongly aliased.
    } else {
        theApp.m_iOnConnectAction = CA_NOACTION;
    }

    // Recorded before connecting: CChatApp::CompleteConnection and the rules engine both read
    // the server name back, and the connection completes asynchronously.
    NativeSetMyServer(server);

    // HrInitAlloc sizes the socket's line buffers. Without it OnReceive has nowhere to put
    // the bytes it reads, so this is not optional even though nothing else calls it here.
    serverConn.HrInitAlloc(2048);

    if (!serverConn.Create()) {
        fprintf(stderr, "session: could not create the socket\n");
        return false;
    }
    GetIrcProto()->SetConnectionStatus(CX_CONNECTING);

    // Non-blocking: FALSE with WSAEWOULDBLOCK means "in progress", which is success as far as
    // starting a connection goes. The real outcome arrives at CIrcSocket::OnConnect, driven by
    // the run loop (native/shim/asyncsocket.cpp).
    BOOL ok = serverConn.Connect(server, (UINT)port);
    if (!ok && GetLastError() != (DWORD)WSAEWOULDBLOCK) {
        fprintf(stderr, "session: connect to %s:%d failed immediately (err %u)\n",
                server, port, (unsigned)GetLastError());
        GetIrcProto()->SetConnectionStatus(CX_DISCONNECTED);
        return false;
    }
    return true;
}

int NativeSessionConnectionStatus() {
    if (!g_started) return CX_DISCONNECTED;
    return GetIrcProto()->GetConnectionStatus();
}

const char* NativeSessionConnectionStatusText() {
    switch (NativeSessionConnectionStatus()) {
        case CX_DISCONNECTED: return "disconnected";
        case CX_CONNECTING:   return "connecting";
        case CX_NOCHANNEL:    return "connected";
        case CX_INCHANNEL:    return "in channel";
        default:              return "unknown";
    }
}

void NativeSessionJoin(const char* channel) {
    if (!g_started || !channel || !*channel) return;
    CRoomInfo info;
    info.m_strChannel = channel;
    GetIrcProto()->ChatJoinChannel(info);
}

void NativeSessionSendToChannel(const char* text) {
    if (!g_started || !text || !*text) return;
    // The engine both transmits this and adds it to the local comic, which is the behaviour
    // the Windows client has: your own line appears in your own comic as it is sent.
    GetIrcProto()->bChatSendToChannel(NULL, text, NULL, 1 /*BM_SAY*/);
}

void NativeSessionRunLoopOnce(double seconds) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, true);
}

int NativeSessionPageCount() {
    return g_doc ? (int)g_doc->m_pages.GetCount() : 0;
}

CPage* NativeSessionPageAt(int n) {
    if (!g_doc) return 0;
    int count = (int)g_doc->m_pages.GetCount();
    if (n < 0 || n >= count) return 0;
    // Reversed, so index 0 is the OLDEST page (reading order) rather than the newest.
    POSITION pos = g_doc->m_pages.GetHeadPosition();
    CPage* page = 0;
    for (int i = 0; i <= (count - 1 - n) && pos; i++) page = (CPage*)g_doc->m_pages.GetNext(pos);
    return page;
}
