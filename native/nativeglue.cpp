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

// --- bodycam.cpp: LoadEmotionStrings ---
// Transcribed from bodycam.cpp:41-47. It is five lines, and the alternative is compiling
// bodycam.cpp - a 1177-line body-cam dialog with 23 GDI call sites - for one function
// the corpus replay genuinely needs (InitHarness calls it before the emotion rules).
//
// emotionName is only read by bodycam's own tooltip and label code, none of which runs
// here, so it lives beside the loader rather than being exported. DELETE BOTH once
// bodycam.cpp compiles.
static CString g_emotionName[9];
void LoadEmotionStrings() {
    int startID = ID_EM_HAPPY;   // assumption in the original: happy first, ids contiguous
    for (int i = 0; i < 9; i++)
        g_emotionName[i].LoadString(startID++);
}

// --- bodycam.cpp ---
// IsSame is REAL, transcribed verbatim from bodycam.cpp:685-695. Same reasoning as
// LoadEmotionStrings above: it is a four-line comparison of member fields with no UI in
// it, and the alternative is compiling bodycam.cpp - a body-cam WINDOW with tooltips,
// mouse capture, SetROP2 XOR drawing and palette handling, none of which the corpus
// replay exercises.
//
// It has to be real rather than aborting because the replay genuinely calls it: pose
// dedup asks whether two bodies are the same, and a wrong answer changes which poses get
// reused and therefore the panel contents. That is exactly what the goldens measure.
BOOL CBodySingle::IsSame(CBody* other) {
    if (!other || GetClass() != other->GetClass()) return FALSE;
    CBodySingle* b = (CBodySingle*) other;
    return (GetPoseID() == b->GetPoseID());
}
RECT CBodySingle::DrawBody(CDC*, RECT&, BOOL) { NATIVE_UNLINKED("CBodySingle::DrawBody (bodycam.cpp)"); }
void CBodySingle::Draw(CDC*, POINT*, RECT*) { NATIVE_UNLINKED("CBodySingle::Draw (bodycam.cpp)"); }
void CBodySingle::GetBodyBox(CPose*, RECT&, RECT&) { NATIVE_UNLINKED("CBodySingle::GetBodyBox (bodycam.cpp)"); }
void CBodySingle::FlipBodyBox(RECT&) { NATIVE_UNLINKED("CBodySingle::FlipBodyBox (bodycam.cpp)"); }
// The two body-cam refresh entry points, called from CAvatarX::UpdateBody whenever a pose
// changes. Both only REDRAW the self-view widgets - they do not touch avatar or pose
// state - so headless behaviour is faithful rather than a placeholder:
//
//   RefreshBodyPreview returns FALSE when there is no character-select body cam
//   (bodycam.cpp:877-884: `if (bcam && av == bcam->m_avatar)`). With no UI there is no
//   such window, so FALSE is the answer the real code would give, and UpdateBody's
//   fallback path is then taken exactly as on Windows.
//
//   RefreshBodyCam assigns bcam->m_avatar and calls RefreshBody, which draws through a
//   CClientDC. With no body cam there is nothing to assign and nothing to draw.
//
// Returning/doing nothing here is therefore not a gap to close later - it is what a build
// with no body-cam window does. If a golden ever depended on this, the dump would differ
// and the corpus diff would say so.
BOOL RefreshBodyPreview(CAvatarX*) { return FALSE; }
void RefreshBodyCam(CAvatarX*) { }

// --- chat.cpp ---
// backdrop.cpp calls this when an unknown backdrop is referenced. The native build
// has no HTTP downloader; aborting is right for now because silently doing nothing
// would leave a backdrop permanently blank with no explanation.
#include "chat.h"
BOOL CChatApp::StartDownloadingBackdrop(LPCSTR, LPCSTR) { NATIVE_UNLINKED("CChatApp::StartDownloadingBackdrop (chat.cpp)"); }

// randfloat now comes from the real balloon.cpp, which links - so the pinned RNG
// sequence is the engine's own again rather than a guard against using it.

// IndexToByte / ByteToIndex now come from the real protsupp.cpp, which links, so the
// transcription that stood here is gone - as its comment said it should be.

// --- protsupp.cpp ---
void SetMyCharacter(const char*) { NATIVE_UNLINKED("SetMyCharacter (protsupp.cpp)"); }

// SetMyPUIAvatarID now comes from the real userinfo.cpp, which links.

// --- bodycam.cpp: CBodyDouble's virtuals, for its vtable ---
// Real, from bodycam.cpp:685-689 - see the note on CBodySingle::IsSame above.
BOOL CBodyDouble::IsSame(CBody* other) {
    if (!other || GetClass() != other->GetClass()) return FALSE;
    CBodyDouble* b = (CBodyDouble*) other;
    return (m_faceRec == b->m_faceRec && m_torsoRec == b->m_torsoRec);
}
RECT CBodyDouble::DrawBody(CDC*, RECT&, BOOL) { NATIVE_UNLINKED("CBodyDouble::DrawBody (bodycam.cpp)"); }
void CBodyDouble::Draw(CDC*, POINT*, RECT*) { NATIVE_UNLINKED("CBodyDouble::Draw (bodycam.cpp)"); }
void CBodyDouble::GetBodyBox(CPose*, CPose*, RECT&, RECT&, RECT&, RECT&) { NATIVE_UNLINKED("CBodyDouble::GetBodyBox (bodycam.cpp)"); }
void CBodyDouble::FlipBodyBox(RECT&, RECT&, RECT&) { NATIVE_UNLINKED("CBodyDouble::FlipBodyBox (bodycam.cpp)"); }
