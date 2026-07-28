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

Across the whole tree: **19 of 91** `.cpp` files compile. The other 72 are
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
     would ripple. A named local was the smaller change.

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

## Milestones

1. **`--avb` natively** — the first real gate, and the reason the engine-core set
   above is what it is. It decodes all 32 ComicArt assets and CRCs 13.8 MB of
   pixels *without drawing anything*, so it runs with the GDI stubs in place and
   can be diffed against the 33 frozen `oracle/avb/*.golden.json`. Byte-identical
   output there proves the shim's type widths, packing, and zlib framing in one
   shot. Blocked on: the project-header long tail (`CCNotif`, `CHARFORMAT`,
   `FindResource`, `Wininet.H`).
2. **The other no-DC dumps** — `--avatario`, `--avatar-pose`, `--codecs` against
   their frozen goldens. `--textpose` additionally needs the `.rc` string table,
   which has no native equivalent; the frozen textpose golden lists the strings,
   so they can be supplied from data.
3. **Core Graphics `CDC`** + the frozen glyph table → the 15 corpus goldens. This
   is the real engineering: ~150 GDI call sites, `StrokeAndFillPath` path
   semantics, and TWIPS mapping.
4. **AppKit shell** — window, comic view, say line. New code, not a `CView`
   emulation.
5. **Networking** — `ircsock` over BSD sockets.
6. **Package** — `.app` bundle installed to `/Applications`.

## Honest scope note

Milestones 1–2 are a tractable grind: the errors are shallow and each fix unlocks
several files. Milestone 3 is where the difficulty is concentrated, and milestone 4
is a from-scratch UI. The ~20k lines of MFC dialogs in the original are mostly
*not* being ported — a minimal native app needs the main window, the comic view,
a say line and a connect dialog, not `autopage`/`rules`/`proppage`/`notipage`. All
the OLE/ActiveX embedding (`bind*`, `oleobjct`, `chatitem`, `chatsrv`, `mfcbind`)
is dropped entirely.
