# Oracle Harness — C++ Console Linkage Design

## Decision

**Option (a-i): Console harness linking the real engine `.obj` files + static
MFC.** The harness is a `/subsystem:console` exe that links every engine `.obj`
from `v2.5-beta-1-modern` (compiled with `-D ORACLE_HARNESS` to activate the
seed hooks) plus the real static MFC library (`/MT`), supplies a stub `theApp`
configuration and a desktop `CClientDC` via the `cui` locator, and drives
`CChatDoc::ProcessLine` / `CUnitPanelPage::AddLine` directly from `main()`.

Option (b) (instrumenting the GUI exe with a `/oracle-dump` flag) was rejected
because it's slower, flakier, harder to make deterministic, and buys nothing
that the console harness doesn't already provide — including GDI text
measurement, which works identically via a desktop `CClientDC` in a console
process.

## Why console-link works

The engine files (`chatdoc`, `panel`, `balloon`, `avatar`, `avatario`,
`textpose`, `histent`, `fonts`, `format`, `avbfile`, `dib`, `backdrop`,
`spline`, `bbox`, etc.) use MFC as **value types + GDI wrappers**
(`CString`, `CPtrList`, `CDWordArray`, `CDC`, `CFont`), never as framework.
The only framework-inheriting class is `CChatDoc : CDocObjectServerDoc`, and
its OLE/doc-template methods are never on the replay hot path.

The critical fact: the DC dependency (the GDI `GetTextExtent` tripwire) is
satisfied by a **single global desktop `CClientDC`** with `MM_TWIPS`
mapping — exactly what the app itself creates at `pageview.cpp:994`. The
harness stuffs this DC into `cui.m_pvClientDC`, and every
`GetClientDC()` macro call in `balloon.cpp`/`fonts.cpp`/`format.cpp`
resolves to it. No GUI session, view, or frame is needed.

`theApp` (a `CChatApp : CWinApp` global defined in `chat.cpp`) is used as a
**service locator** for ~12 config fields (`m_comicsFont`, `m_comicsColor`,
`m_iFontHeightBalloon`, `m_bNoRefresh`, `m_flags1`, etc.). The harness calls
`AfxWinInit()` (for MFC internal init), sets those fields directly, and
calls the individual init functions (`LoadEmotionStrings`,
`InitializeEmotionRules`, `InitializeBackDrops`, `InitializeAvatars`) that
`CChatApp::InitInstance` would normally call — without ever running the
MDI/OLE doc-template/message-pump machinery.

## Build

From the `v2.5-beta-1-modern` directory, after `vcvars32.bat`:

```
nmake /f oracle.mak CFG="oracle - Win32 Release"
```

This compiles all engine `.cpp` files with `/D ORACLE_HARNESS` into a
separate `Release/oracle/` intermediate directory (so it doesn't clash with
the normal `chat.mak` build), then links them with the harness sources into
`Release/OracleHarness.exe`.

## Usage

```
OracleHarness.exe <inputs.json> <expected.json>
OracleHarness.exe --glyphs <glyphs.json>
```

The `--glyphs` mode dumps the Comic Sans MS glyph-advance table and
`CFontInfo` scalars at the pinned font size, for the TS port to consume
verbatim.

## What the harness does NOT do

- No GUI window, no MDI frame, no doc-template, no OLE activation.
- No `CChatApp::InitInstance()` call — init functions are called directly.
- No view (`CChatView`/`CPageView`/`CTextView`). `cui.m_pvChatView` is NULL.
  The harness calls `ProcessLine` directly, bypassing `SayEntry::Execute`'s
  `GetView()->GetDocument()` indirection.
- No network/IRC. Messages come from `inputs.json`.

## Files

| File | Purpose |
|---|---|
| `oracleharness.cpp` | Console `main()`: reads `inputs.json`, replays messages, dumps `expected.json` |
| `oracle.mak` | nmake file (in `v2.5-beta-1-modern/`) that builds `OracleHarness.exe` |
| `ojson.h` / `ojson.cpp` | Minimal deterministic JSON DOM (parse + emit) |
| `oracleseed.h` / `oracleseed.cpp` | Determinism layer: seed pinning + seed-usage ledger |

## Known limitations / TODO(oracle)

- `CChatDoc`'s protected constructor is accessed via a public-ctor subclass
  `COracleChatDoc`. If `IMPLEMENT_DYNCREATE` / `CRuntimeClass` issues arise
  at link time, the fallback is to friend the harness or patch the ctor to
  public under `#ifdef ORACLE_HARNESS`.
- The harness links ALL engine `.obj`s (including GUI files like `MainFrm`,
  `chatView`, etc.) to satisfy cross-reference link dependencies. These
  files' code paths are never executed — they're dead weight in the binary
  but needed for link resolution. If link errors appear from GUI-only
  symbols, they'll be stubbed here.
- `CTraj` dump is a placeholder (the `traj.h` struct layout needs to be
  examined to dump tail/arrow points properly).
- Spline `bezpts` array bounds are estimated from `BezierCount()`; may need
  adjustment after first CI run.