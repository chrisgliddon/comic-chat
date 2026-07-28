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
BOOL CChatApp::StartDownloadingBackdrop(LPCSTR, LPCSTR) { NATIVE_UNLINKED("CChatApp::StartDownloadingBackdrop (chat.cpp)"); }

// randfloat now comes from the real balloon.cpp, which links - so the pinned RNG
// sequence is the engine's own again rather than a guard against using it.

// IndexToByte / ByteToIndex now come from the real protsupp.cpp, which links, so the
// transcription that stood here is gone - as its comment said it should be.

// --- protsupp.cpp ---
void SetMyCharacter(const char*) { NATIVE_UNLINKED("SetMyCharacter (protsupp.cpp)"); }

// SetMyPUIAvatarID now comes from the real userinfo.cpp, which links.

