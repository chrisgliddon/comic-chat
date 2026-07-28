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

Across the whole tree: **25 of 91** `.cpp` files compile. `balloon`, `panel`,
`histent` and `fonts` have joined the engine-core set — that is the balloon
geometry/outline unit and the panel layout unit, which between them own most of the
Tier-3 goldens. The other 72 are
predominantly the MFC dialogs and the OLE/ActiveX embedding that are not being
ported at all. The engine files still blocked — `balloon`, `panel`, `pageview`,
`chatdoc`, `histent`, `protsupp`, `ircsock`, `fonts` — all fail on the same kind of
transitively-included UI types (`CPrintInfo`, `CCoolToolBar`, `OFN_*`), i.e. more
of the same shim grind rather than anything structural.

Working:

- Compilation is from **unmodified** engine source apart from five small
  portability fixes listed below.
- The MSVC CRT RNG is reproduced bit-exactly (`native/shim/msvcrand.cpp`);
  `srand(1)` → 41 and `srand(0)` → 38, the two values pinned in the port tests.
  This matters more than it looks: panel layout and avatar placement consume
  `rand()`, and every corpus golden depends on the sequence.
- Only **five** source incompatibilities in ~92k lines, all fixed with forms MSVC
  also accepts, so the Windows build is unaffected (and CI verifies that):
  1. `sizeof MATRIX` without parentheses, twice in `spline.cpp`.
  2. `DashSeg(POINT&, DASHINFO&)` called with a temporary — MSVC binds temporaries
     to non-const references. `DashSeg` only reads the point, so it became
     `const POINT&` (definition in `traj.cpp`, local declarations in `arc.cpp` and
     `spline.cpp`).
  3. `avatar.cpp` reused a loop variable after its `for` scope ended. Both
     makefiles pass `/Zc:forScope-`, MSVC's pre-standard scoping, which clang has
     no equivalent for; the second loop now declares its own `i`.
  4. `GetBodyFromEmotion(CEmotion(0.0, 0.0))` — same temporary-to-non-const-ref
     extension, but the signature is virtual and overridden twice, so widening it
     would ripple. A named local was the smaller change (two sites: `avatar.cpp`,
     `panel.cpp`).
  5. **`/Zc:forScope-` reuse, seven sites** across `avatar.cpp`, `panel.cpp` and
     `balloon.cpp`. Both makefiles pass that switch, so a variable declared in a
     `for` header stays in scope afterwards. Where the variable is dead after the
     loop the second loop declares its own; where it is **read** after the loop —
     `balloon.cpp:1674` and `:1729`, `panel.cpp`'s star labels — the declaration is
     **hoisted** instead, because scoping it per-loop would change behaviour rather
     than just satisfy the compiler. Checking which case applied, one site at a time,
     was the whole job here.
  6. **`/Zc:strictStrings-`**, one site: `chatdoc.cpp:259` binds a string literal to
     `char*` inside a conditional. clang has no equivalent switch and the conversion
     is in an expression, so an explicit cast was needed; MSVC accepts the cast form
     unchanged.

- **`CString` is a single `char*` member, and that is load-bearing.** The engine
  passes `CString` into printf-style varargs in ~580 places. MFC gets away with it
  because a `CString` *is* one pointer to a NUL-terminated buffer. An earlier shim
  held `std::string` members: it compiled, and every one of those 580 sites would
  have pushed a `std::string` and read its internals as a `char*` — silently. The
  build also passes `-Wno-error=non-pod-varargs`, which is sound *only* because of
  that layout.

- `-fms-compatibility` is deliberately **not** used: it redefines `va_list` against
  macOS system headers and breaks every translation unit. `-fms-extensions` alone
  is what the engine needs (for redundant member qualification).

- **`min`/`max` are defined at the very END of `stdafx.h`, after every standard
  header.** libc++ `#undef`s them to protect `std::min`/`std::max`, so defining them
  earlier — as an earlier version of this shim did, in `win32types.h` — silently
  loses them again. They must stay macros rather than `std::min`/`std::max` because
  the engine mixes int/short/double operands freely, which the templates reject.

  Found by CI on its first run, not locally: this machine has Apple clang 21 where
  the definitions survived, while the `macos-14` runner's clang 15 erased them and
  three core files stopped compiling. Worth remembering as the shape of bug the
  native job exists to catch — a shim that works on one toolchain and not another.

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

Two scaffolding files exist only for this milestone and should shrink, not grow:

- `native/nativeglue.cpp` supplies `theApp` and `cui` as **raw zeroed storage** under
  the right assembler names, because their real constructors live in `chat.cpp` and
  `ui.cpp` and pull in the whole MFC application tree. Nothing reads them. When a
  milestone genuinely needs application state, the answer is a real native
  application object — not a bigger stub.
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
