// nativeglue.cpp - the globals the engine objects reference but the Tier-2 dump
// never uses.
//
// Four symbols are needed to link the .avb path: theApp, cui, g_puiSelf and
// g_bCanViewUnrated. None is reached by the dump - it goes straight at the files
// through CAvatarFileStream and never touches the application object, the UI
// singleton or the self-user pointer.
//
// theApp and cui are supplied as RAW STORAGE under the right assembler names
// rather than as constructed objects, because their real constructors live in
// chat.cpp and ui.cpp, which pull in the whole MFC application and view tree. The
// storage is oversized and zeroed.
//
// This is a deliberate tripwire, not a shortcut: reading through either of these
// would be reading zeroed memory as an object, so the moment a milestone genuinely
// needs application state (theApp.GetAvatarDir(), the comics font, the backdrop
// dirs) the answer is to build a real native application object - NOT to grow this
// file. `--avb` was chosen as milestone 1 partly because it needs none of it.

#include "stdafx.h"

// theApp and cui now come from native/nativeapp.cpp as REAL objects. This file
// used to supply them as raw zeroed storage under asm labels, which was enough for
// the asset dumps (they never read application state) but not for the corpus replay,
// which needs fonts and art directories. See nativeapp.cpp for what its constructor
// does and does not reproduce.

// The global palette pair the engine's stdafx.h declares. A default-constructed
// CPalette is inert (the shim's is a stub), and pageview.cpp only takes its address
// to hand to CDC::SelectPalette, which is itself a no-op until there is a real
// graphics backend.
CPalette   ghPalette;
LOGPALETTE *gpLogPal = 0;

// iBytesofChar (intl.c:2699) reduces to `if (!g_pMime) return 1;` and g_pMime is the
// CJK/MIME converter, which the native build never initialises - the double-byte path
// is the deferred Tier-1 #13 scope question. Both callers (balloon.cpp:196,
// proppage.cpp:782) are additionally gated behind `GetMime() ?`, so this reproduces
// the only branch that can be taken here.
//
// Defined rather than linked because intl.c does not compile natively yet: it is C,
// and it collides with the macOS <ctype.h> internals through the shim. If CJK support
// is ever wanted, this must come from intl.c instead - a 1 for a lead byte would
// split a character in half mid-line.
extern "C" int iBytesofChar(BYTE) { return 1; }

// Also weak. 96 keeps DpiScale as the identity and matches the dpi the glyph table was
// captured at - those two must agree or pixel surfaces scale away from the advances
// beside them.
__attribute__((weak)) int g_screenDpi = 96;

// The WinInet DLL handle, defined in chat.cpp. NULL: nothing loads WinInet, and
// urlutil.cpp checks it before use.
HINSTANCE g_hinstWinInet = 0;

// These two have trivial types, so they get honest definitions.
// WEAK definitions. The small dump drivers (verify.sh) link neither protsupp.cpp nor
// userinfo.cpp, so they need these; the full harness links both and its STRONG
// definitions win. Without the weak attribute, adding protsupp to one link produced
// duplicate symbols while removing it from the other produced undefined ones - there is
// no single non-weak arrangement that satisfies both binaries.
__attribute__((weak)) BOOL g_bCanViewUnrated = FALSE;
class CUserInfo;
__attribute__((weak)) CUserInfo* g_puiSelf = 0;

// ===========================================================================
// Leaf symbols that avatar.o and avatario.o import from engine files not yet in
// the native link: panel.cpp (CPanelElement), bodycam.cpp (CBodySingle, the
// body-cam refreshers), balloon.cpp (randfloat), chat.cpp
// (StartDownloadingBackdrop).
//
// Every one of them ABORTS rather than returning a plausible value. The --avb dump
// reaches none of them: it goes straight at the files and never lays out a panel,
// draws a body, or draws from the RNG. So reaching one means the dump has started
// doing something it should not, and a quiet stub would turn that into subtly wrong
// output instead of a stack trace.
//
// randfloat is the sharpest case. It consumes the MSVC LCG, and the corpus goldens
// pin the exact draw sequence (RULEBOOK 4), so a stub returning 0.0 would silently
// desynchronise every later panel. Aborting is the only safe answer until
// balloon.cpp itself links.
//
// These disappear as the real files compile - they are a scaffold for milestone 1,
// not a permanent layer.
// ===========================================================================

#include "bbox.h"
#include "pe.h"
#include "dib.h"
#include "avatar.h"
#include "resource.h"   // ID_EM_HAPPY

#define NATIVE_UNLINKED(what) \
    do { \
        fprintf(stderr, "native: %s is not linked yet (%s:%d)\n", (what), __FILE__, __LINE__); \
        abort(); \
    } while (0)

// CPanelElement now comes from the real panel.cpp, which links.

// --- intl.c: GetMime ---
// REAL, and load-bearing. intl.c holds it as `static PSCRIPTINFO g_pMime; void *GetMime()
// { return g_pMime; }` and only SetMime ever assigns it - and only when
// _IsFECodePage(GetACP()) is true, i.e. for a Far East code page. This build is CP-1252, so
// g_pMime is never set and NULL is the value the real code produces.
//
// Why it matters: GetMime() is the switch for the whole international path.
// FindFurthestLineBreak (balloon.cpp:289) and balloon.cpp:391 both test `if (GetMime())`
// before calling FindFurthestLineBreakIntl, and FindSubStringForINTLThatFits dereferences
// pMime->iCp with NO null check (intl.c:530). So NULL keeps the engine on the single-byte
// line-breaking path the goldens were captured on; anything non-NULL takes a path that
// crashes immediately. It is also why intl.c is not compiled - with GetMime() NULL, nothing
// in it is reachable from the measurement path.
//
// (Deleted once by accident while removing the bodycam stubs, which turned three corpus
// cases into crashes inside the generated stub. Worth the size of this comment.)
extern "C" void* GetMime() { return NULL; }

// --- bodycam.cpp ---
// Nothing here any more. bodycam.cpp COMPILES now, so LoadEmotionStrings, both IsSame
// implementations, the CBodySingle/CBodyDouble drawing virtuals and RefreshBodyCam/
// RefreshBodyPreview are all the engine's own code rather than transcriptions or stubs.
// That was worth doing for the drawing ones in particular: a body composites head and torso
// poses with optional masks in an order set by the avatar's TORSOFIRST/TORSOMASK/HEADMASK
// flags, and re-deriving that would have been guesswork where the file was right there.

// --- chat.cpp ---
// backdrop.cpp calls this when an unknown backdrop is referenced. The native build
// has no HTTP downloader; aborting is right for now because silently doing nothing
// would leave a backdrop permanently blank with no explanation.
#include "chat.h"
#include "memblst.h"    // CMemberList, for the participant-pane no-ops at the end
BOOL CChatApp::StartDownloadingBackdrop(LPCSTR, LPCSTR) { NATIVE_UNLINKED("CChatApp::StartDownloadingBackdrop (chat.cpp)"); }

// randfloat now comes from the real balloon.cpp, which links - so the pinned RNG
// sequence is the engine's own again rather than a guard against using it.

// IndexToByte / ByteToIndex now come from the real protsupp.cpp, which links, so the
// transcription that stood here is gone - as its comment said it should be.

// --- protsupp.cpp ---
// SetMyCharacter is real now - see the identity block at the end of this file.

// SetMyPUIAvatarID now comes from the real userinfo.cpp, which links.


// --- setupdlg.cpp: the identity accessors -------------------------------------------
// REAL storage, not stubs. These are the user's nickname, real name, ident and server, and
// the IRC layer reads them back when it builds its registration: CIrcSocket::HrIrcLogin sends
//
//     NICK <GetMyName()>
//     USER <m_pszUserName> ... :<GetMyRealName()>
//
// so a stub aborts the moment a connection is attempted, and one returning "" would register
// with an empty nick and be rejected by the server.
//
// On Windows these live in setupdlg.cpp, which is 3000 lines of property-sheet dialogs backed
// by the registry. The native front end owns identity instead - it is app configuration, not
// engine behaviour - so this is the storage those dialogs would have written to. Defaults are
// chosen so a session works before anything has been configured.
namespace {
CString g_myNick     = "comicchat";
CString g_myIdent    = "comicchat";
CString g_myRealName = "Comic Chat (macOS)";
CString g_myUserName = "comicchat";
CString g_myServer;
CString g_myChannel;
CString g_myCharacter = "bolo";
CString g_myHomePage;
CString g_myEmail;
}

const char* GetMyName()           { return (LPCSTR)g_myNick; }
const char* GetMyScreenName()     { return (LPCSTR)g_myNick; }
const char* GetMyNickName()       { return (LPCSTR)g_myNick; }
const char* GetMyIdent()          { return (LPCSTR)g_myIdent; }
const char* GetMyRealName()       { return (LPCSTR)g_myRealName; }
const char* GetMyUserName()       { return (LPCSTR)g_myUserName; }
const char* GetMyServer()         { return (LPCSTR)g_myServer; }
const char* GetMyPhysicalServer() { return (LPCSTR)g_myServer; }
const char* GetMyCharacter()      { return (LPCSTR)g_myCharacter; }
const char* GetMyChannel()        { return (LPCSTR)g_myChannel; }
const char* GetMyHomePage()       { return (LPCSTR)g_myHomePage; }
const char* GetMyEmail()          { return (LPCSTR)g_myEmail; }

void SetMyName(const char* s)      { if (s) g_myNick = s; }
void SetMyNameNick(const char* s)  { if (s) g_myNick = s; }
void SetMyIdent(const char* s)     { if (s) g_myIdent = s; }
void SetMyRealName(const char* s)  { if (s) g_myRealName = s; }
void SetMyCharacter(const char* s) { if (s) g_myCharacter = s; }
void SetMyHomePage(const char* s)  { if (s) g_myHomePage = s; }

// Set by the session when a connection is started, so the rules engine and the status line
// can report which server this is.
void NativeSetMyServer(const char* s) { if (s) g_myServer = s; }

void GetMyServerDisplayName(CString& str) { str = g_myServer; }

// GetMyServerPrettyName - the server name as shown to the user. On Windows this looks up a
// friendly label in the server list ("MSN Chat" for a comicsrv host); with a direct connection
// the hostname the user typed IS the pretty name.
void GetMyServerPrettyName(CString& str) { str = GetMyServer(); }

// --- rules.cpp: the automation engine ------------------------------------------------
// Comic Chat lets a user define RULES ("when someone joins, greet them", "ignore this nick")
// and NOTIFICATIONS, edited through property sheets and stored in the registry. rules.cpp is
// 3600 lines of that, most of it dialog code.
//
// A session with no rules configured is the normal case, and its answer is "nothing matched" -
// which is what these return. That is faithful rather than a placeholder: the engine calls
// bMatchAndApplyRules on every join, leave, connect and message, and on Windows with an empty
// rule set it returns FALSE too.
//
// Only the ARGUMENTS are read (they are copied into the m_*Cach members before matching), so
// returning FALSE without touching them loses nothing.
BOOL CCDynaRules::bMatchAndApplyRules(enumEvents, enumActions*, enumActions*,
                                      CString&, CString&, CString&, CString&) {
    return FALSE;
}
// GetFlags is defined inline in rules.h, so it is already available.
BOOL CCDynaRules::bDaemonNeeded() { return FALSE; }
BOOL CCDynaRules::bStartRulesDaemon(UINT, BOOL) { return FALSE; }
BOOL CCDynaNotifs::bDaemonNeeded() { return FALSE; }
BOOL CCDynaNotifs::bStartNotifsDaemon(UINT, BOOL) { return FALSE; }

// --- chatsrv.cpp / motd.cpp / protsupp.cpp: settings and secondary UI ------------------
// AddToServerList maintains the recent-servers list that the connect dialog offers. Returns
// NULL - "not added" - because the native front end has no server list yet: the user supplies
// a hostname directly. Nothing on the connection path uses the return value.
class CChatService;
CChatService* AddToServerList(LPCSTR) { return 0; }

// UpdateSpectators refreshes the member-list pane's spectator section. No pane yet, and it
// touches no engine state, so doing nothing is faithful rather than deferred.
void UpdateSpectators(CChatDoc*, BOOL) {}

// ShowMOTD opens the message-of-the-day window. The MOTD text is still received and kept by
// CIrcSocket (m_strMOTD), so nothing is lost - only the window is missing.
void ShowMOTD(const char* szLUsers, const char* szMOTD) {
    if (szMOTD && *szMOTD) fprintf(stderr, "[motd] %.400s\n", szMOTD);
    (void)szLUsers;
}

// The identity-mask matcher, used by the rules engine to decide whether a user matches a
// pattern like "nick!*@*.example.com". With no rules configured (see bMatchAndApplyRules
// above) nothing asks it to match, so "no match" is consistent rather than arbitrary.
BOOL bGetUserMatchFromMask(LPTSTR, void*) { return FALSE; }
BOOL bIsMatch(void*, LPCTSTR, LPCTSTR, LPCTSTR) { return FALSE; }

// --- setupdlg.cpp: ArtDirsOK ---------------------------------------------------------
// Transcribed from setupdlg.cpp:1704. It answers "do the art directories hold at least one
// backdrop AND at least one character", and it must be REAL rather than hardcoded TRUE,
// because a wrong answer is silently destructive in both directions:
//
//   FALSE when art exists  -> AdjustViewMode (protsupp.cpp:152) reads
//                             `if (g_iViewMode == VM_TEXT || !theApp.m_bFoundArt)` and sends
//                             the engine into the TEXT view on every channel join.
//   TRUE when art is absent -> the comic view runs with no characters to draw.
//
// A backdrop counts as either .bmp or .bgb, exactly as the original does - .bgb came later and
// the check accepts both.
BOOL ArtDirsOK() {
    struct _finddata_t fd;
    UINT found = 0;
    const UINT FOUND_BACKDROPS = 1, FOUND_AVATARS = 2;

    CString pattern = theApp.m_strBackDropDir + "\\*.bmp";
    long h = _findfirst((char*)(const char*)pattern, &fd);
    if (h != -1) { found |= FOUND_BACKDROPS; _findclose(h); }

    if (!(found & FOUND_BACKDROPS)) {
        pattern = theApp.m_strBackDropDir + "\\*.bgb";
        h = _findfirst((char*)(const char*)pattern, &fd);
        if (h != -1) { found |= FOUND_BACKDROPS; _findclose(h); }
    }

    pattern = theApp.m_strAvatarDir + "\\*.avb";
    h = _findfirst((char*)(const char*)pattern, &fd);
    if (h != -1) { found |= FOUND_AVATARS; _findclose(h); }

    return found == (FOUND_BACKDROPS | FOUND_AVATARS);
}

// ChatSetChannel / ChatSetServer - remember the room and server for next time. On Windows these
// write to the registry through setupdlg.cpp; here they update the same storage the GetMy*
// accessors read, which is what the engine consults afterwards.
void ChatSetChannel(const char* szChannel) { if (szChannel) g_myChannel = szChannel; }
void ChatSetServer(const char* szServer)   { if (szServer)  g_myServer = szServer; }

// --- utils.cpp: CNCSMapStringToPtr ---------------------------------------------------
// Transcribed from utils.cpp:1653-1695. "NCS" is non-case-sensitive: it lowercases the key
// before delegating to CMapStringToPtr, because IRC nicknames are case-insensitive.
//
// The exception matters and is easy to miss. A key beginning with an apostrophe is an ENCODED
// nickname - EncodeNick (ircproto.cpp) emits a leading ' for names that needed UTF-8 escaping,
// and DecodeNick tests for it. Those are compared verbatim, because lowercasing an escaped
// name would change the escape sequence itself.
//
// This is the nick -> CUserInfo map the message path uses (m_mapNickToPtr), so it has to be a
// real implementation: a stub means no incoming message can ever find its sender.
BOOL CNCSMapStringToPtr::Lookup(LPCTSTR key, void*& rValue) const {
    if (!key) return FALSE;
    if (key[0] == '\'') return CMapStringToPtr::Lookup(key, rValue);
    CString strLowerKey = key;
    strLowerKey.MakeLower();
    return CMapStringToPtr::Lookup(strLowerKey, rValue);
}

void CNCSMapStringToPtr::SetAt(LPCTSTR key, void* newValue) {
    if (!key) return;
    if (key[0] == '\'') { CMapStringToPtr::SetAt(key, newValue); return; }
    CString strLowerKey = key;
    strLowerKey.MakeLower();
    CMapStringToPtr::SetAt(strLowerKey, newValue);
}

BOOL CNCSMapStringToPtr::RemoveKey(LPCTSTR key) {
    if (!key) return FALSE;
    if (key[0] == '\'') return CMapStringToPtr::RemoveKey(key);
    CString strLowerKey = key;
    strLowerKey.MakeLower();
    return CMapStringToPtr::RemoveKey(strLowerKey);
}

// --- memblst.cpp: CMemberList ---------------------------------------------------------
// The participant pane - the "mugshots in the upper right" of the original UI. There is no
// such pane yet in the native front end, so these three are display-only operations with
// nothing to display to.
//
// They are faithful rather than arbitrary. GetSortPosition (memblst.cpp:330) walks the list
// control comparing sort keys and returns m_MemberListBox.GetItemCount() when it finds no
// earlier slot; with no control that count is 0, so 0 is precisely what the real code returns
// for an empty list. Sort and MakeVisible reorder and scroll the control, which is meaningless
// without one.
//
// Participant TRACKING is unaffected: that lives in the document's m_mapNickToPtr and the
// CUserInfo objects, which are real. Only the on-screen list is missing.
int  CMemberList::GetSortPosition(CUserInfo*) { return 0; }
void CMemberList::Sort() {}
void CMemberList::MakeVisible(CUserInfo*) {}

// --- protsupp.cpp / wmini.cpp: the whisper box and sound -------------------------------
// The whisper box is a separate little window that collects whispered messages. bAddToWhisperBox
// returns TRUE when it has CONSUMED the message, and ProcessSay reads it as
//     if (!bAddToWhisperBox(pui, pui->m_udi.m_uModes, szMesg)) { ...put it in the comic... }
// so FALSE is not a placeholder - it is the answer that sends every message to the comic, which
// is what a build with no whisper box should do. Returning TRUE would silently swallow messages.
BOOL bAddToWhisperBox(CUserInfo*, USHORT, const char*) { return FALSE; }
BOOL bWhisperInBox(CString, CString, CDWordArray*, USHORT) { return FALSE; }

// Sound effects. FALSE means "no sound was played", which is true.
BOOL bFindAndPlaySound(const char*, BOOL) { return FALSE; }

// bLegalToSend gates outgoing text. TRUE because the native front end only offers to send when
// a connection exists; the Windows version also checks flood control and a modal-dialog guard,
// neither of which applies here.
BOOL bLegalToSend(BOOL) { return TRUE; }
