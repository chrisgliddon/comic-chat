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

// g_screenDpi backs dpiscale.h's DpiScale(). The original sets it in
// CChatApp::InitInstance from the display; 96 here, which makes DpiScale the identity
// and matches the dpi the glyph table was captured at. Those two must agree: a
// different value would scale pixel surfaces away from the advances they sit beside.
int g_screenDpi = 96;

// The WinInet DLL handle, defined in chat.cpp. NULL: nothing loads WinInet, and
// urlutil.cpp checks it before use.
HINSTANCE g_hinstWinInet = 0;

// These two have trivial types, so they get honest definitions.
BOOL g_bCanViewUnrated = FALSE;
class CUserInfo;
CUserInfo* g_puiSelf = 0;

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

#define NATIVE_UNLINKED(what) \
    do { \
        fprintf(stderr, "native: %s is not linked yet (%s:%d)\n", (what), __FILE__, __LINE__); \
        abort(); \
    } while (0)

// --- panel.cpp ---
CPanelElement::CPanelElement(const CPanelElement& o) { m_bbox = o.m_bbox; }
BOOL CPanelElement::SetBBox(int, int, int, int) { NATIVE_UNLINKED("CPanelElement::SetBBox (panel.cpp)"); }
void CPanelElement::GetBBox(RECT*) { NATIVE_UNLINKED("CPanelElement::GetBBox (panel.cpp)"); }

// --- bodycam.cpp ---
BOOL CBodySingle::IsSame(CBody*) { NATIVE_UNLINKED("CBodySingle::IsSame (bodycam.cpp)"); }
RECT CBodySingle::DrawBody(CDC*, RECT&, BOOL) { NATIVE_UNLINKED("CBodySingle::DrawBody (bodycam.cpp)"); }
void CBodySingle::Draw(CDC*, POINT*, RECT*) { NATIVE_UNLINKED("CBodySingle::Draw (bodycam.cpp)"); }
void CBodySingle::GetBodyBox(CPose*, RECT&, RECT&) { NATIVE_UNLINKED("CBodySingle::GetBodyBox (bodycam.cpp)"); }
void CBodySingle::FlipBodyBox(RECT&) { NATIVE_UNLINKED("CBodySingle::FlipBodyBox (bodycam.cpp)"); }
void RefreshBodyCam(CAvatarX*) { NATIVE_UNLINKED("RefreshBodyCam (bodycam.cpp)"); }
void RefreshBodyPreview(CAvatarX*) { NATIVE_UNLINKED("RefreshBodyPreview (bodycam.cpp)"); }

// --- chat.cpp ---
// backdrop.cpp calls this when an unknown backdrop is referenced. The native build
// has no HTTP downloader; aborting is right for now because silently doing nothing
// would leave a backdrop permanently blank with no explanation.
#include "chat.h"
BOOL CChatApp::StartDownloadingBackdrop(LPCSTR, LPCSTR) { NATIVE_UNLINKED("CChatApp::StartDownloadingBackdrop (chat.cpp)"); }

// --- balloon.cpp: see the RNG note above ---
double randfloat() { NATIVE_UNLINKED("randfloat (balloon.cpp) - would desync the pinned RNG sequence"); }

// --- protsupp.cpp: IndexToByte / ByteToIndex ---
// Transcribed from protsupp.cpp:1023-1032, not stubbed: the avatario dump encodes
// every emotion through them, so an abort would make milestone 2 unreachable and a
// wrong value would corrupt the wire bytes.
//
// They are two-line ASCII digit conversions and the avatario golden exercises the
// full table, so a transcription error fails immediately rather than lurking. This
// is still duplication - DELETE BOTH once protsupp.cpp links natively.
BYTE IndexToByte(BYTE byteIn) { return byteIn + '0'; }
BYTE ByteToIndex(BYTE byteIn) { return byteIn - '0'; }

// --- protsupp.cpp ---
void SetMyCharacter(const char*) { NATIVE_UNLINKED("SetMyCharacter (protsupp.cpp)"); }

// --- userinfo.cpp ---
// Stubbed rather than linking userinfo.o: that object compiles, but it imports the
// history and protocol layer (AddAndExecute, GetMembers, GetMyNickName, IsIgnored,
// the HistoryEntry vtables), which would pull most of the tree into a dump that
// needs none of it.
void SetMyPUIAvatarID(UINT) { NATIVE_UNLINKED("SetMyPUIAvatarID (userinfo.cpp)"); }

// --- bodycam.cpp: CBodyDouble's virtuals, for its vtable ---
BOOL CBodyDouble::IsSame(CBody*) { NATIVE_UNLINKED("CBodyDouble::IsSame (bodycam.cpp)"); }
RECT CBodyDouble::DrawBody(CDC*, RECT&, BOOL) { NATIVE_UNLINKED("CBodyDouble::DrawBody (bodycam.cpp)"); }
void CBodyDouble::Draw(CDC*, POINT*, RECT*) { NATIVE_UNLINKED("CBodyDouble::Draw (bodycam.cpp)"); }
void CBodyDouble::GetBodyBox(CPose*, CPose*, RECT&, RECT&, RECT&, RECT&) { NATIVE_UNLINKED("CBodyDouble::GetBodyBox (bodycam.cpp)"); }
void CBodyDouble::FlipBodyBox(RECT&, RECT&, RECT&) { NATIVE_UNLINKED("CBodyDouble::FlipBodyBox (bodycam.cpp)"); }
