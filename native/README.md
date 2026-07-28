# Native macOS port

Goal: a real macOS application built from the **original C++ engine source**, not
a rewrite and not a wrapper. `native/shim` supplies the Win32/MFC floor so the
engine `.cpp` files compile under clang on arm64; the front end will be new AppKit
code that calls the engine.

## Status (2026-07-28)

**All 15 engine-core files compile natively** — `vector2d`, `bbox`, `traj`, `arc`,
`spline`, `splinutl`, `dib`, `avbfile`, `backdrop`, `avatar`, `avatario`,
`textpose`, `format`, `userinfo`, `doskey`. Run `native/build.sh` for the live
table, `native/build.sh <file>` for one file's full errors.

That set is the whole `.avb`/DIB asset pipeline (`avbfile` + `dib` + `avatar` +
`backdrop`), the emotion/pose logic (`avatario`, `textpose`, `avatar`), the
geometry core (`vector2d`, `bbox`, `traj`, `arc`, `spline`, `splinutl`) and the
line-breaking unit (`format`).

Across the whole tree: **29 of 91** `.cpp` files compile, including `chatdoc` (the
document model), `pageview`, `balloon`, `panel` and `protsupp`.

**Every engine file the corpus replay needs now compiles, and so does the oracle
harness itself** — 29 of 91 across the tree. The corpus replay is therefore a linking
problem now, and the surface has been measured rather than guessed at:

**158 undefined symbols**, and every one is in a subsystem this port had already
decided to drop:

| count | category |
| --- | --- |
| 53 | MFC dialogs, property pages, windows, controls |
| 88 | other UI/app methods — `CChatApp::OnDisconnect`, `DoModalDlg`, whisper boxes, room-info management |
| 5 | rules / notifications engine |
| 4 | NetMeeting / conference |
| 3 | CJK / INTL (`GetMime`, `bSB2DBKatakana`) — the deferred Tier-1 #13 |
| 2 | sound / MCI |
| 3 | globals (`serverConn`, `g_rgIrcCmd`, …) |

None is engine geometry, layout or asset handling — those all compile and link. So the
next step is a stub file in the established shape: **abort on call, not silently
no-op**, so that running a corpus case names whichever one the replay actually reaches
instead of quietly producing a wrong dump. That list is also the honest inventory of
what a native app would have to replace with AppKit rather than port.

The `protsupp` work is worth reading before touching similar files, because the two
failed batch attempts taught more than the successful pass:

- **12 `CString` temporaries bound to non-const references** (7 × `bMatchAndApplyRules`,
  5 × `bChatSendText`). Widening the parameters to `const CString&` would have been one
  edit instead of twelve, but it cascades through `iGetFirstMatchingRule` into
  `rules.cpp` — which does not compile natively, so the cascade could not be checked.
- One of those sites was **the single statement of a brace-less `if` that has an
  `else`**. Declaring a local before it re-pairs the `else` and silently changes
  control flow, so braces had to be added. A scripted "wrap each `CString()`" pass
  cannot see that.
- Another was **inside a `switch`**, where a per-case declaration would cross the other
  case's initialisation; the local was hoisted above the `switch` instead.
- Two had a **concatenation** as the argument (`CString(szSender)+"!"+szFullName`), so
  the local captures the whole expression.
- `protsupp.cpp:1960` passed **three arguments to the two-parameter `MAKELONG`** (a
  stray trailing comma). MSVC's legacy preprocessor tolerates extra macro arguments and
  drops the empty third, so removing the comma is value-identical — a latent bug the
  original compiler hid.
- `iArg` was another `/Zc:forScope-` hoist, and one conditional was **ambiguous**
  (`const char*` and `CString` convert both ways).

### The dead `extern` at `DoChannelDialog`

`CRoomInfo::DoChannelDialog` opened with `extern CString strCurrentChannelTopic;`,
while `chatprot.h:101` defines that name as an object-like **macro**
`(currentRoom->m_strTopic)` — so it expanded to
`extern CString (currentRoom->m_strTopic);`, which MSVC accepts and clang rejects.

Rather than guess, this was settled by **preprocessing the file with `oracle.mak`'s own
flags in CI**: the expansion is present in the `.i`, the two uses below expand to the
same member access, and the identifier never survives unexpanded anywhere — so no
global of that name exists and nothing referred to the declaration. It was dead, and
removing it is behaviour-preserving.

Two wrong turns on the way to that, both the same mistake — concluding from a probe
that could not support the conclusion. The first probe invented its own compiler flags
and died in `afxver_.h` (`_AFXDLL` without `/MD`), telling me about my flags rather than
the code. The second reported "the macro never expands" from a grep truncated at
`-First 5`, which could not have shown the expansion 630,000 lines later.

## Why staging, not include paths

`native/stage.sh` builds a symlink farm of the engine sources beside the shim's
`stdafx.h`. A quoted `#include "stdafx.h"` resolves relative to the including
file's own directory *before* any `-I`, so compiling in place always finds
`v2.5-beta-1-modern/stdafx.h` — which has no include guard to define around, and
which pulls in `afxwin`/`afxext`/`afxole`/`afxsock` plus `chicdial.h` and
`coolbar.h`. That last part is the blocker: it drags the MFC **UI** surface into
every translation unit, including the many that have no business needing it.

## Why this is the right shape

The oracle already built for the TypeScript port is language-agnostic JSON, so it
validates a native C++ port too — and because this approach *reuses* the engine
source rather than reimplementing it, the oracle becomes a regression check on the
**shim layer** instead of on a rewrite. That is a far weaker fidelity risk, and it
moves the feedback loop off Windows CI onto local compiles.

Two shim decisions follow directly from that and should not be relaxed:

- **Integer widths.** Win32 is ILP32, macOS arm64 is LP64. `avbfile.h` typedefs
  `AVBINT32` from `ULONG` inside `#pragma pack(push, 1)` and memcpys the structs
  straight out of the file, so `ULONG` must be `uint32_t`, never `unsigned long`.
  On this platform `long` is never the right spelling for a Win32 32-bit type.
- **Measurement stubs abort; painting stubs no-op.** `CDC::GetTextExtent` feeds
  `CFormatInfo` line breaking, which the corpus goldens pin exactly — a stub
  returning zero would silently reflow every balloon. When implemented it must
  read the **frozen glyph table** (`oracle/glyphs/glyphs.json`), not Core Text;
  measuring live reintroduces exactly the platform dependence the frozen table
  exists to remove (RULEBOOK 5).

## Milestones 1 and 2: PASSED — 35 goldens reproduced byte-for-byte

`native/verify.sh` builds the native dumps and diffs them against the frozen
Windows goldens. Current result: **matched 35, mismatched 0** — the 33 Tier-2 asset
manifests plus Tier-1 #3 (avatario) and #2 (avatar-pose).

That single test covers 32 assets, 552 poses, 1612 decoded image slots and ~13.8 MB
of hashed pixels, and it confirms:

- **Integer widths and struct packing.** `avbfile.h` typedefs `AVBINT32` from
  `ULONG` inside `#pragma pack(push, 1)` and memcpys the record structs straight
  out of the file. Had `ULONG` been `unsigned long` (8 bytes here, 4 on Win32),
  every offset in every asset would have been read from the wrong bytes.
- **zlib framing, the ditto-optimisation, and the 2-bpp maskedmono/dualmask
  expansion** — the paths that turn 745 stored images into 1612 decoded slots.
- **`DIBStorageWidth`'s sub-byte rounding** and the row-masked pixel hash.
- **`COLORREF` channel order** surviving the port, via the palette CRCs.

The comparison is strong because both sides run the *same* dump code
(`oracle/harness/avbdump.cpp`, extracted from the harness for exactly this reason):
a match cannot be two implementations coincidentally agreeing, and a mismatch is
attributable to the platform layer rather than to the dump.

One real portability bug surfaced and was fixed in that shared code: it built paths
with a backslash separator, which Win32 accepts but macOS treats as an ordinary
filename character — the first native run produced 33 files literally named
`avbout\anna.json`. Now `/`, which both platforms accept.

## The native application object

`native/nativeapp.cpp` now supplies **real** `theApp` and `cui` objects, replacing the
raw zeroed storage the asset dumps used. The corpus replay reads fonts, art
directories and flags, so it needs the real thing.

`CChatApp::CChatApp` there is a **documented subset** of `chat.cpp:157-277`. The
member initialisations are transcribed faithfully — several are load-bearing
(`m_flags1 = ~0` carries `F1_RTFCOMIC`, `m_charSet`, `m_strDefaultArtDir`). Omitted,
each for a stated reason: the rules/notifications wiring (function pointers into
files that don't compile, and the replay drives `ProcessLine` directly), the GUI font
(no Win32 stock objects, and the frozen table pins the *comics* font anyway), the
image list, and the service connector.

**The omissions are an experiment with a referee, not an assumption.** The Windows
oracle links the real `chat.cpp`, so its goldens encode the real constructor. The 35
goldens still reproducing proves the omitted parts don't affect those dumps; when the
corpus dump lands, a mismatch will name whichever one matters. That is what makes a
subset worth attempting rather than making a 2871-line UI file compile first — it
converts a lift into a measurement.

Its by-value members (`CCDynaRules`, `CCRulesData`, `CCDynaNotifs`, `CCDelayedRules`,
`CChatServiceConnector`, `CRtfCtrl`) get **no-op constructors** rather than aborts —
uniquely in this port, because they must succeed for `theApp` to exist at all. Their
*methods* are undefined, so anything that actually drives those subsystems fails to
link, naming what it pulled in.

Other scaffolding that should shrink, not grow:
- The same file stubs leaf symbols from files not yet linked (`panel.cpp`'s
  `CPanelElement`, `bodycam.cpp`'s body classes, `balloon.cpp`'s `randfloat`). Every
  one **aborts** rather than returning a plausible value. `randfloat` is the sharpest
  case: it consumes the MSVC LCG whose draw order the corpus goldens pin, so a stub
  returning `0.0` would silently desynchronise every later panel.

Milestone 2 is worth separating from milestone 1 because it tests different things.
The asset manifests exercise byte layout and decode; avatario and avatar-pose are
pure arithmetic, so a clean diff there isolates float-to-double widening, the
**tree-dependent `PI`** used by the angle metric (RULEBOOK 1.2 — taking it from
`M_PI` would shift the nearest-pose search), and the MSVC RNG.

One duplication was accepted to get there and is marked for deletion:
`IndexToByte`/`ByteToIndex` are transcribed into `native/nativeglue.cpp` from
`protsupp.cpp:1023-1032`. They are two-line ASCII digit conversions, the avatario
golden exercises the full table so a transcription error fails immediately, and
stubbing them to abort would have made milestone 2 unreachable. **Delete both once
`protsupp.cpp` links natively.**

## Remaining milestones

2b. **`--codecs`** against its golden — blocked on `ircsock.cpp` compiling.
   **`--textpose`** needs the `.rc` string table, which has no native equivalent;
   the frozen textpose golden lists the strings, so they can be supplied from data.
3. **Core Graphics `CDC`** → the 15 corpus goldens. ~150 GDI call sites,
   `StrokeAndFillPath` path semantics, TWIPS mapping. The real engineering.

   **Text measurement is DONE** (`native/shim/glyphtable.{h,cpp}`,
   `glyphtable_cdc.cpp`): `CDC::GetTextExtent`, `GetTextMetrics`, `GetCharWidth` and
   `GetDeviceCaps` all read the frozen table, never Core Text. `native/glyphcheck`
   verifies 19 properties through the CDC API and runs first in `verify.sh`.

   Two safety properties worth preserving:
   - **A font the table does not cover aborts rather than measures.** `SelectObject`
     records whether the selected `LOGFONT` matches the pinned one; measuring with a
     different font would produce a plausible wrong number, which is worse than a
     crash because it only surfaces as a golden mismatch layers downstream. The
     engine does use other fonts (titles, UI), so this will fire — and when it does
     it names the font to add to `CaptureGlyphs`.
   - **An unpinned byte aborts rather than being skipped.** Skipping would silently
     shorten the string and move the line break.
4. **AppKit shell** — window, comic view, say line. New code, not a `CView`
   emulation.
5. **Networking** — `ircsock` over BSD sockets.
6. **Package** — `.app` bundle installed to `/Applications`.

## Honest scope note

Milestone 2 is a tractable grind: the errors are shallow and each fix unlocks
several files. Milestone 3 is where the difficulty is concentrated, and milestone 4
is a from-scratch UI. The ~20k lines of MFC dialogs in the original are mostly
*not* being ported — a minimal native app needs the main window, the comic view,
a say line and a connect dialog, not `autopage`/`rules`/`proppage`/`notipage`. All
the OLE/ActiveX embedding (`bind*`, `oleobjct`, `chatitem`, `chatsrv`, `mfcbind`)
is dropped entirely.
