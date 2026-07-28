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

    InitializeBackDrops();
    LoadEmotionStrings();
    InitializeEmotionRules();
    InitializeAvatars();

    g_doc = new CNativeChatDoc();
    cui.m_pvChatDoc = g_doc;      // GetChatDoc() finds it here
    g_doc->m_view = NULL;         // see CNativeChatDoc
    g_doc->m_bComicView = TRUE;
    g_doc->SetComicsTitle("Comic Chat");
    g_doc->InitMyDocument();

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
