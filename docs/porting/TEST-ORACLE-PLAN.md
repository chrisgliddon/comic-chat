# Comic Chat → PixiJS Port: Test-Oracle Coverage Review & Plan

*Status of the test suite as of 2026-07-24, and the plan to build the characterization
oracle required for a mechanical port, following the Goal/Loop methodology
(tests-first, language-agnostic corpus, maker/verifier loops, differential referee).*

---

## 1. Verdict

**The repository contains zero unit tests and zero E2E tests.** The only automated
verification is `.github/workflows/build-modern.yml`: it builds both `*-modern` trees
on `windows-2022` and runs a packaging smoke test (launch the GUI for 8 s, assert a
top-level window exists). There is no conformance oracle of any kind today — it must
be built before any porting loop can run.

The good news from the codebase review:

1. **The comic engine is far more testable than a 1996 MFC app has any right to be.**
   The text→emotion rules, emotion→pose selection, panelization, placement/flip
   logic, and all spline/bbox geometry are pure or near-pure logic with clean seams.
2. **The engine is deterministic by design** — panel layout reseeds a single PRNG
   from a stored per-panel seed (`panel.cpp:867` `srand(m_seed)`) precisely so replay
   reproduces the same comic. Only two leaks break this (see §4).
3. **The app ships its own replay format** (`.ccc` conversation files, `histent.cpp`)
   that already encodes gesture/expression/mode/talk-to per message — a ready-made
   corpus container.
4. **The comic math is essentially identical across all four version snapshots**
   (spline/bbox/arc/vector2d/traj/textpose/semantic differ by ≤24 cosmetic lines
   between v1.0-pre and v2.5b1), so oracle work is not hostage to the version choice.

---

## 2. What actually needs porting (engine vs. shell)

Corrections discovered during review (do not trust file names):

| File | Reality |
|---|---|
| `rules.cpp`, `autopage.cpp` | **Bot automation / "chat rules" feature + its settings UI.** Not the comic engine. Optional for MVP. |
| `semantic.cpp` | Dead code (`#if 0` SIGGRAPH demo hacks). |
| `textcore.cpp` | RichEdit text-view wrapper, not balloon text. |
| `wmini.cpp` | Balloon word-wrap/spline drawing ("word mini"), not settings. |
| `cache.cpp` | Dead stub (doesn't even compile). Ignore. |

**The real engine (MUST port, MUST pin):**

- `textpose.cpp` — text → emotion rules (`GetEmotionsFromString`, textpose.cpp:271). Pure, deterministic, resource-string rule tables.
- `avatar.cpp` / `avatario.cpp` — emotion → pose nearest-neighbor (`GetBodyFromEmotion`, avatar.cpp:354/386), emotion-wheel quantization (`EmotionToBytes`/`BytesToEmotion`, `emFloats[]` avatario.cpp:45).
- `panel.cpp` — panelization (`CUnitPanelPage::AddLine`, panel.cpp:1058), new-panel decisions (panel.cpp:1079), avatar ordering/flip/greedy placement (panel.cpp:358-435), zoom, balloon packing (`LayoutBalloons`, panel.cpp:855).
- `balloon.cpp` — line breaking (`BreakIntoLines`), balloon spline outline (`GetFilters`→`PermuteFilters`→`AddWavies`→`CBeta`).
- `spline.cpp`, `splinutl.cpp`, `bbox.cpp`, `arc.cpp`, `vector2d.cpp`, `traj.cpp` — pure math, version-identical. Port first.
- `histent.cpp` — history model + `.ccc` serialization (`SayEntry::FormatOtherArgs`/`ReadOtherArgs`, histent.cpp:143-218).
- `chatdoc.cpp` — the document model + `ExecuteHistory` replay (chatdoc.cpp:737), `ProcessLine`/`FindAttribution`/`FindPose` (chatdoc.cpp:447/375/405).
- `pageview.cpp` — page-layout math only (`GetProspectivePanelWidth` pageview.cpp:1143, panels-per-row, interstices, pagination).
- `avbfile.cpp` / `dib.cpp` / `backdrop.cpp` — asset formats (see §6).
- `protsupp.cpp` / `ircsock.cpp` / `ircproto.cpp` — protocol codecs (see §5).

**Shell (rebuild natively in the web app, no mechanical port):** `mainfrm`, `childfrm`,
`chatview`, `textview`, `saywnd`, `rtfctrl`, `rtfcmb`, all dialogs; `nmproto.cpp`
(NetMeeting, `#ifdef`'d out); SSPI auth; ident server.

---

## 3. Port baseline decision (Goal-phase; needs sign-off)

**Recommendation: port the engine from `v1.0-pre-modern`; use one tree — suggest
`v2.5-beta-1-modern` — as the single golden oracle.**

- Engine math is identical across generations; `v1.0-pre`'s `balloon.cpp` (33 KB) is
  the readable reference vs. v2.5's RTF/intl-encrusted 56 KB version.
- The two modern trees do **not** produce identical comics. Deliberate divergences:
  - v2.5-modern default balloon font is **12 pt** vs. original/v1.0 **9 pt** (`chat.rc:2336`).
  - v1.0-pre-modern has a balloon word-wrap fix (long words no longer split mid-word); v2.5 wraps via its own intl-aware breaker.
  - Both have panels-per-row auto-fit (`FitPanelsWide`, pageview.cpp:1394) replacing the fixed "2 wide".
- Whichever tree is the oracle, **goldens come from exactly one tree** and the harness
  must pin: window size / `SetPanelsWide(n)`, 96 DPI, font size, and the RNG seed.

---

## 4. Determinism preconditions (Stage 0 — blockers, do these first)

The engine is reproducible **iff** all of the following are handled:

1. **Seed the seeding chain.** `CChatDoc` captures `m_seed = rand()` (chatdoc.cpp:182)
   and reseeds via `srand(m_seed)` (chatdoc.cpp:206); each `CPanel` captures `m_seed = rand()`
   (panel.cpp:556) and `LayoutBalloons` reseeds per panel (panel.cpp:867). Harness must
   force a fixed initial seed.
2. **Neutralize the one true entropy source:** `CAvatarComplex::SetSequential` seeds
   from `GetTickCount()` (avatar.cpp:974-975) when auto-assigning avatars to new users.
   Override/pin in the harness.
3. **Close the persistence gap:** `StartHistoryEntry::WriteSelf` (histent.cpp:531) does
   **not** persist `m_randStart` — a `.ccc` file does not capture the seed. The harness
   must inject a fixed seed on replay (or we patch the dump to record it).
4. **Replicate the MSVC CRT LCG exactly** in TypeScript:
   `seed = seed*214013 + 2531011; return (seed>>16) & 0x7fff` (`RAND_MAX = 0x7FFF`),
   consumed via `randfloat()` (balloon.cpp:428). **Call order is load-bearing** —
   including the zero-effect `ShiftLines` calls (balloon.cpp:760/768, MAXSHIFT==0) that
   still advance the sequence. Live consumers: goal width/lines/startX
   (panel.cpp:899/903/916) and `GetRandomTitle` (panel.cpp:469).
5. **Text metrics are the dominant cross-platform hazard.** Balloon line breaking calls
   GDI `GetTextExtent` with Comic Sans MS throughout (`balloon.cpp:367,668,670` etc.;
   `format.cpp:707+`; `fonts.cpp:53`). The port must use a **pinned glyph-advance table**
   captured from the oracle (per font/size/DPI), never live browser measurement, or
   line breaks — and everything downstream — diverge. Goldens must record the
   `CFontInfo` scalars (`m_leading`, `m_lineHeight`, `m_baseAdd`,
   `m_continuationWidth`, `m_topOffset`, balloon.h:47).
6. **Express goldens in world units** (panel ≈ 4860 logical units, TWIPs page space),
   not device pixels, so DPI/raster differences don't pollute the corpus.

Non-render clock uses (safe to ignore): flood timer `time(NULL)` rules.cpp:1763; print
footer `GetLocalTime` pageview.cpp:514.

---

## 5. The test inventory (what "comprehensive" means here)

### Tier 1 — Pure-logic unit tests (run against C++ oracle harness AND the TS port)

| # | Target | Source | Notes |
|---|---|---|---|
| 1 | Text → emotion set (`GetEmotionsFromString` → full `CEmotionOpts`) | textpose.cpp:271 | Highest-value single unit. Cover caps→SHOUT, smileys, I/You→POINT, greetings→WAVE, `!!!`, LOL/ROTFL, priorities. |
| 2 | Emotion → pose (`GetBodyFromEmotion` incl. `m_lastBody/Face/Torso` history) | avatar.cpp:354/386 | Run **sequences**, not single messages — the hidden state matters. |
| 3 | Emotion quantization round-trip (`EmotionToBytes`/`BytesToEmotion`) | avatario.cpp:45 | Guards protocol + replay boundary. |
| 4 | UDI annotation codec round-trip (`bInsertAnnotations` ↔ `ProcessUDIData`/`ProcessSay`) | protsupp.cpp:3057/1485/1545 | Both IRCX `DATA…CCUDI1` and inline `(#G…E…R M T…)` forms; byte-exact `value+'0'` encoding. |
| 5 | Identity comments (`ProcessComment`: Appears as / GetInfo / HeresInfo / GetCharInfo / BDrop / BDrop2) | protsupp.cpp:846-1020 | Incl. `"?"` deferred-URL sentinel and `.`/`,` delimiter quirks. |
| 6 | IRC line parse (`ParseIt` → `IRCPARSE`) | ircsock.cpp:137-258 | No-prefix, numeric codes, MAXARGS spill, RFC2812 JOIN-without-colon (modern fix ircsock.cpp:1361). |
| 7 | Formatting codec (`SzControlLess`/`SzControlFull`/`nFillFormatting`, color map) | format.cpp:303/517/459/862 | Pin the documented `^k33`→`^k0133` hack. |
| 8 | URL detection (`HrIdentifyUrls` + prefix/suffix/boundary fns; `FIsURL`, `mic://` scheme) | urlutil.cpp:119-390, urlfind.cpp:199-360 | **Bug-for-bug:** `IHexToInteger` uses `\|\|` where `&&` belongs (urlfind.cpp:313-326) — pin current behavior, `BUG(port):` marker. |
| 9 | Mode maps `SM2BM`/`BM2SM`, `IndexToByte`/`ByteToIndex` | protsupp.cpp:1023-1063 | Cheap; underpins #4. |
| 10 | `.ccc` transcript codec (`FormatOtherArgs`/`ReadOtherArgs`, all 9 entry keywords) | histent.cpp:112-717 | Byte-exact WriteSelf goldens + parse∘serialize identity. |
| 11 | Geometry math: vector2d (ROUND half-away-from-zero), bbox ops, Bézier subdivision, Cardinal/Beta spline conversion | vector2d/bbox/splinutl/spline.cpp | Property + golden tests; version-identical, port first. |
| 12 | `CDosKey` history ring buffer | doskey.cpp | Wraparound/modulo edge cases. |
| 13 | JIS↔Shift-JIS tables, CJK wrap predicates | jis2sjis/sjis2jis.cpp, intl.c | Only if CJK path is in scope. |

### Tier 2 — Asset/format golden tests (the shipped corpus is the fixture set)

45 `.avb` avatars + 9 `.bgb` + 2 `.bmp` ship in `comicart/` + `artpack1/` (~5 MB).
Four characters (bolo, cro, denise, lynnea) exist in **two independent encodings** —
free cross-checks. `artpack1/archive/` holds uncompressed pre-pack sources.

- **`.avb` parse → canonical JSON manifest**, frozen as snapshots: header, name/flags/URLs,
  global palette, face/torso/body tables (poseID, emotion idx, intensity, x/y, cx/cy,
  deltas, faceX/faceY), per-pose image slots {format, palette type, w, h, bpp,
  CRC32 of **decoded pixels**}. Reader spec: `avbfile.cpp:743+`; spec-of-record doc:
  `artifacts/docs/cchat/avfiles.obd` ("Avatar File Specs", Word). Writer for round-trip
  oracle: `artifacts/avtools/`.
- **Pin the tricky decode paths:** ditto-optimization (equal `dwImageOffset` reuses pose,
  avbfile.cpp:1093+), `AK_OFFSET_ADJUSTMENT` fixups (avbfile.cpp:947), 2-bpp
  maskedmono/dualmask → 3×1-bpp expansion lookup tables (avbfile.cpp:1471-1542) —
  render pose drawing+mask+aura composites to PNG snapshots.
- **DIB codec:** RLE4/RLE8 expansion (dib.cpp:888-1015), OS/2 `BITMAPCOREHEADER`,
  3-byte BGR palettes (avbfile.cpp:250), zlib framing {u32 sizes} + level-9 deflate,
  stride check (avbfile.cpp:712).
- **`.bgb` both branches** ('BM' plain DIB vs `0x8181` container, avbfile.cpp:1669);
  note world-coordinate metadata is **hardcoded, not in-file** (backdrop.cpp:28-40:
  315×315, world 4860×-4860, normHeight 100) — port the constants, pin them in a test.
- **`.ccr` locator** parse (setupdlg.cpp:892-980).

### Tier 3 — Engine golden-state dumps (sequence-driven; the heart of the oracle)

For scripted message sequences (seeded), dump and freeze per message, in order of
increasing brittleness:

1. Panel membership + new-panel boolean (drivers at panel.cpp:1079: ≥5 balloons,
   speaker-already-present, ACTION, `<Brk>`, overflow spill).
2. Left-to-right avatar order, per-body `m_flip`, and historesis state
   (`m_lastDir/m_lastLeft/m_lastRight`) after each panel.
3. Per-panel `zoomFactor` + per-body {w, h, top, m_arrowX}; `Establishing()` gate.
4. Line-break output (`CFormatInfo`: m_nLines, rgiLengths, rgiWidths, offsets) —
   **the GDI tripwire**; divergence cascades into everything below.
5. Balloon `m_bbox`/`m_trueBox`/`m_routeRgn` + tail/arrow points.
6. Balloon outline control points (`cps[]`/`bezpts[]` from the CBeta spline).
7. Avatar hidden state before/after each message (`m_lastBody/Face/Torso`, flags).
8. Full-panel raster hash — last resort, only after 1–7 are green and the glyph
   table is frozen.

### Tier 4 — E2E differential (the referee loop)

- Corpus case = `inputs.json` (seed, avatar assignments, scripted message stream with
  modes/talk-tos, panel width/columns, font scalars) + `expected.json` (Tier-3 dumps
  per message + final page layout). Schema shared by both runners.
- **C++ runner:** a small console harness on Windows CI linking the engine files
  (windows-2022 runner already builds the trees; smoke test already launches the GUI).
  OLE automation **cannot** inject messages or read state (it's a menu-command
  dispatcher, bindauto.cpp:520) — so either (a) link engine objs into a harness exe,
  or (b) drive the real exe via a loopback IRC feed and capture via automation `Save`
  (.ccc) plus an added state-dump hook. Prefer (a); (b) validates the seams (a) bypasses.
- **TS runner:** loads the same `inputs.json`, runs the ported engine headless (no
  PixiJS needed for logic tiers), dumps the same schema.
- Diff per message; **first divergent message + field** is the signal. Failures group
  by root cause; recurring failures amend the rulebook, not the files.

---

## 6. Judge validation (before any porting loop runs)

Per the methodology, the oracle itself must be proven:

1. **Passes the original:** C++ harness run twice with the same seed → byte-identical
   dumps (proves the determinism work in §4 actually landed).
2. **Fails broken code:** mutate one rule in `textpose.cpp` (e.g., the caps→SHOUT
   threshold) and one constant in `panel.cpp` → assert the corpus goes red. A judge
   that can't catch breakage isn't a judge.
3. **Assertions can't be weakened:** corpus + expected dumps live outside the loop's
   write scope; adversarial review on any test/fixture diff.

---

## 7. Known bug-for-bug ledger (grows during porting; policy: reproduce + `BUG(port):`)

| Site | Behavior to preserve |
|---|---|
| urlfind.cpp:313-326 | `IHexToInteger` `\|\|`-vs-`&&` bug — invalid hex accepted/rejected per current logic. |
| balloon.cpp (v2.5) | Long-word wrap splits mid-word (fixed only in v1.0-pre-modern) — match whichever tree is baseline. |
| format.cpp:502-508 | `^k33`→`^k0133` color-code hack. |
| backdrop.cpp:28-40 | World metadata hardcoded, not read from file. |
| avbfile.cpp:1093+ | Ditto-pose optimization changes pose identity (poseID reuse). |

---

## 8. Phased execution plan (mapped to the loops doc)

| Phase | Work | Benchmark to proceed |
|---|---|---|
| **0. Determinism** (Goal) | Fixed-seed injection, GetTickCount pinning, seed persisted in dumps, glyph-advance table captured from oracle, 96-DPI/columns pinned | Same seed → identical dumps twice |
| **1. C++ oracle harness** (Goal) | Console harness on windows-2022 CI linking engine files; JSON dump of Tier-3 structs; corpus schema | Judge validation §6 passes |
| **2. Corpus generation** (Loop) | Scripted sequences: happy path, 1..6 speakers, all modes, panel-break triggers, all 45 avatars, edge cases (empty msg, 500-char msg, unicode, `<Brk>`, whisper/think/action) | Every Tier-1/3 behavior has ≥1 pinned fixture; deliberately-broken code reddens |
| **3. Rulebook** (Goal) | MFC/GDI→TS/PixiJS idiom map (CString/CPtrList→native, GetTextExtent→glyph table, StrokeAndFillPath→PIXI.Graphics, DIB+mask/aura→RGBA textures, TWIPs→world-unit container transform, CRT rand→LCG class); 3-file shakedown port | Two agents translate the same construct identically |
| **4. Port loops** (Loop) | Order: vector2d/bbox/splinutl/spline → textpose → avatario/avatar → balloon/panel → histent/chatdoc → pageview → avbfile/dib → protsupp/ircsock codecs. Maker + 2 adversarial reviewers + fixer per unit; corpus read-only | Tier-1/2 green per module |
| **5. Differential** (Loop) | Both runners over full corpus; drive diff to zero; first-divergence tracing | 0 diffs, tests verified actually ran |
| **6. Idiomize + web shell** | PixiJS rendering layer, WebSocket IRC bridge, UI — corpus stays green per commit | Corpus green on every commit |
