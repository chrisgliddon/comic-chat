# Oracle Progress Ledger

*Resumable memory for the Comic Chat characterization-oracle work
(Phases 0–2 of `docs/porting/TEST-ORACLE-PLAN.md`). Update per unit of work.*

## Status

| Phase | State | Gate status |
|---|---|---|
| 0. Determinism | DONE | PASSED — two runs byte-identical (CI run 30126575092) |
| 1. C++ harness | DONE | PASSED — builds, runs, all 10 corpus cases produce dumps |
| 2. Corpus | DONE | PASSED — 10 cases frozen as goldens; golden comparison green on every push |
| 3. Rulebook | DONE | PASSED — rulebook + 3-file shakedown port; two-agent check 67/67 bezpts match |
| 4. Port loops | IN PROGRESS | textpose + avatario + avatar ported + oracle-golden verified (32 + 23 + 17 probes match) |

Judge validation §6: all three gates PASSED.
- (a) same seed → byte-identical (CI 30126575092 determinism step)
- (b) mutate rules → corpus red (CI 30127051147, divergence at balloon bbox)
- (c) revert → green (CI 30126575092 corpus step, all 10 GOLDEN MATCH)

## Decisions

- Golden oracle tree: `v2.5-beta-1-modern` (sole source of goldens).
- Harness linkage strategy: **Console harness linking engine objs + static MFC
  (option a-i).** Decision rationale in `oracle/harness/README.md`. The engine
  uses MFC as value types + GDI wrappers only; the DC dependency is satisfied
  by a desktop `CClientDC(MM_TWIPS)` in the `cui` locator, identical to the
  app's runtime strategy. No GUI session needed.
- All oracle changes flag-gated (`ORACLE_HARNESS` define / command-line flag);
  no default runtime behavior change to the published modern trees.
- Corpus cases 001-010 cover: happy path, all modes, panel-break triggers,
  1-6 speakers, edge cases, emotion triggers, speaker-present break, overflow,
  multi-panel pagination, avatar variety.
- Goldens committed with LF line endings enforced via `.gitattributes` so
  byte-identity comparisons are stable across Windows/macOS/Linux checkouts.
- The corpus step compares fresh runs against frozen goldens (actual.json vs
  expected.json) so regressions are caught. To re-freeze after an intentional
  engine change, delete expected.json, run CI, commit actual.json as
  expected.json.

## Done

- 2026-07-24: `docs/porting/TEST-ORACLE-PLAN.md` committed on `oracle/phase-0`.
- 2026-07-24: Phase 0 determinism hooks wired into engine (see prior LEDGER versions).
- 2026-07-24: Phase 1 harness + schema + scripts + CI (see prior LEDGER versions).
- 2026-07-24: Phase 2 corpus cases 001-010 written.
- 2026-07-24: **Fixed LayoutAvatars crash** (root cause: `av->m_userInfo` was
  NULL in the headless harness; `EvalPair`/`AddTalkTos` dereference it during
  `OrderAvatars`). The harness now synthesizes a `CUserInfo` per loaded avatar
  and assigns it to `av->m_userInfo` (`oracleharness.cpp:LoadAvatarByName`).
- 2026-07-24: **Fixed Establishing() crash** (`pageview.cpp:832` called
  `GetView()->GetDocument()` but the harness has no view; under
  `ORACLE_HARNESS` it now uses `GetChatDoc()` directly).
- 2026-07-24: **Fixed bezpts overread** (dump loop read `bezCount*2` entries
  but the array has exactly `BezierCount()`; overread captured uninitialized
  memory and broke determinism).
- 2026-07-24: **Fixed neutral-body bbox nondeterminism** (`CPanelElement`
  ctor only sets `m_bbox.Left/Right`, not `Top/Bottom`; the avatar's neutral
  body bbox is never set in the headless path since the body cam isn't run.
  Dropped the neutral-body bbox from `DumpAvatarState`; the per-panel body
  bboxes — set by `CUnitPanel::LayoutAvatars` via `SetBBox` — are the Tier-3
  #3 target and are dumped in `DumpPanel`).
- 2026-07-24: **Fixed avatar loading in corpus 002-010** (`SetArtDir` in
  `protsupp.cpp:164` constructs the avatar dir as `m_strBaseDir + "\\" +
  m_strDefaultArtDir`, so the harness must pass a RELATIVE name ("ComicArt")
  not a full path; added `treeDir="."` to all corpus cases).
- 2026-07-24: **Fixed validate.mjs** (schema path `../schema`; use
  `ajv/dist/2020` for draft 2020-12 `$defs` schemas).
- 2026-07-24: **Freeze goldens** — 10 `expected.json` + `glyphs.json` from CI
  run 30125674590 committed. Golden comparison (actual vs expected) added to
  the `oracle.yml` corpus step via pwsh.
- 2026-07-24: **Judge validation §6b PASSED** — mutated
  `maxBodyHeight /1.9 → /2.5` (panel.cpp) and `normHeight 100 → 120`
  (avatar.cpp) on a scratch branch; corpus went red (divergence at
  `$.messages[0].page.panels[1].balloons[0].bbox.left`); reverted → green.
- 2026-07-24: **Removed verbose crash-debugging logging** (FetchSpeaker rec
  dump, LayoutAvatars granular stepping, SetNeutral rec dump). CI stayed
  green (run 30127357571).
- 2026-07-24: **Phase 3 — Rulebook + shakedown port COMPLETE.** Wrote
  `docs/porting/RULEBOOK.md` (the MFC/GDI→TS/PixiJS idiom map). Ported
  `vector2d`/`bbox`/`spline`+`splinutl` to `port/` (ESM TS + Vitest, 75/75
  tests, typecheck clean). Two-agent check PASSED: 67/67 CBeta bezpts match
  `oracle/corpus/001/expected.json` byte-exact. Discovered the load-bearing
  `ROUND` semantics: MSVC `((int)(fp+0.5))` behaves as `floor(fp+0.5)` (not
  `trunc`) — pinned in `port/src/core/numeric.ts` and RULEBOOK §1.
- 2026-07-24: **Phase 4 — textpose ported + oracle-golden verified.** Ported
  `textpose.cpp` (GetEmotionsFromString + rule compiler) with the v2.5 frozen
  rule strings inlined. Added `--textpose` dump mode to the C++ harness
  (Tier-1 #1) with a 32-probe battery; froze `oracle/textpose/textpose.golden.json`
  from CI run 30129560316. The TS port matches the oracle 32/32 probes
  byte-exact (121/121 total tests pass). The oracle golden exposed two
  rulebook issues, both fixed: (1) the `EM_*` emotion constants use the v2.5
  full-precision PI + `Math.fround` float-cast (not the v1.0-pre 3.14159);
  (2) a C engine bug at textpose.cpp:300-313 where the sentence-start loop
  uses `buff`/`lower` (whole-string start) instead of `bptr`/`lptr` —
  reproduced bug-for-bug (`BUG(port)`).
- 2026-07-24: **Fixed oracle harness: link chat.res.** The harness exe was
  NOT linking the compiled resource segment, so `LoadString(ID_RULE_*)`
  returned empty strings and `InitializeEmotionRules` registered zero rules.
  The `--textpose` dump exposed this (all probes returned empty opts). Added
  `chat.res` compilation + link to `oracle.mak`. The corpus goldens did NOT
  diverge (the loaded rules don't change the captured dumps for the existing
  corpus messages). CI run 30129560316 confirms all gates green.
- 2026-07-24: **Phase 4 — avatario ported + oracle-golden verified.**
  Ported `avatario.cpp` (the emotion-wheel quantization module:
  `emFloats[]` table, `IndexToByte`/`ByteToIndex`, `EmotionToBytes`/
  `BytesToEmotion` round-trip, `EmotionToFloat`). Added the 5 missing
  sentinel emotion constants (`EM_DOUBLEPOINT`/`EM_SHRUG`/`EM_3QRWALK`/
  `EM_SIDEWALK`/`EM_3QFWALK`) to `port/src/core/emotion.ts`. Added the
  `--avatario` C++ harness dump mode (23-probe battery covering every
  emFloats[] entry at intensity 1.0, out-of-range emotion NEUTRAL fallback,
  intensity truncation sweep). Froze `oracle/avatario/avatario.golden.json`
  from CI run 30131211049; the TS port matches byte-exact on every probe
  and the emFloats[] table itself. Added the avatario capture + golden
  comparison step to `oracle.yml`. Fixed two harness build errors: (1)
  `EmotionToBytes`/`BytesToEmotion` are forward-declared in avatario.cpp:66-67
  but NOT in avatario.h, so the harness needed local `extern` decls; (2)
  `emFloats` is declared `extern float emFloats[]` (no size) in avatario.h,
  so `sizeof(emFloats)` is illegal from the harness TU — added a
  `kEmFloatsCount=18` constant.
- 2026-07-24: **Phase 4 — avatar ported + oracle-golden verified.** Ported
  the `GetBodyFromEmotion` family from `avatar.cpp` (Tier-1 #2: emotion
  → pose). The port covers `CBody`/`CBodySingle`/`CBodyDouble`,
  `CAvatarSimple`/`CAvatarComplex`, the record types `RBODYREC`/
  `FACEREC`/`BODYREC`, and all four `GetBodyFromEmotion` variants
  (single-emotion + CEmotionOpts for both Simple and Complex) plus the
  neutral-fallback helpers. Added the `--avatar-pose` C++ harness dump
  mode that constructs SYNTHETIC avatars on the stack with known
  bRec/fRec (no real .avb loading — that's Phase 4 avbfile work) and
  runs a 17-probe battery (9 simple + 8 complex). Froze
  `oracle/avatar/avatar.golden.json` from CI run 30131970285; the TS
  port matches byte-exact on the integer indices and the post-state
  `m_last*`. The dump exposed two C quirks which the port reproduces
  bug-for-bug (documented in RULEBOOK §14.4): (1) the angle-after-
  normalization coincidence where sentinel inputs (e.g. EM_WAVE=1001)
  can match a directional bRec entry whose normalized angle falls
  under the PI/8 threshold; (2) `GetHeadAndBodyFromEmotion` sets only
  ONE index per call (face for directional, torso for sentinel), so
  the CEmotionOpts path always falls back to NEUTRAL for the torso
  when the input is directional. The PI-precision issue (spline
  golden pinned to v1.0-pre PI=3.14159; oracle uses v2.5 full precision
  PI=3.14159265358979323846) was discovered and resolved by adding
  local `PI_V25` / `value_to_angle_v25` / `subtract_angles_v25` to
  `port/src/engine/avatar.ts` (the threshold `PI/8` is sensitive to
  PI precision; this is now a load-bearing rulebook entry, §14.2).
  Also caught a real port bug: the initial port wrote `m_lastBody`/
  `m_lastFace`/`m_lastTorso` at the end of each `GetBodyFromEmotion`
  call, but the C only writes these in the consumer's `UpdateBody` →
  `RecordBody` flow (avatar.cpp:755-770). Removed those writes; the
  dump's fresh-avatar pattern leaves m_last* at -1. All 181/181 tests
  pass, typecheck clean. CI run 30132205263 confirms all gates green
  including the new avatario + avatar golden comparisons.

## CI runs of record

| Date | Run | Purpose | Result |
|---|---|---|---|
| 2026-07-24 | 30121643636 | First build+replay | Build PASS, crash in AddLine |
| 2026-07-24 | 30121990457 | Diagnose crash | Crash in OrderAvatars (m_userInfo NULL) |
| 2026-07-24 | 30124347126 | After CUserInfo + Establishing fixes | Harness fully runs; determinism failed (bezpts overread) |
| 2026-07-24 | 30125225003 | After bezpts + SetArtDir + schema fixes | Determinism PASS, all 10 corpus run, schemas VALID |
| 2026-07-24 | 30125674590 | First fully green | Build ✓ Determinism ✓ Corpus ✓ Schemas ✓ Artifacts ✓ |
| 2026-07-24 | 30126575092 | Golden comparison + .gitattributes | All 10 GOLDEN MATCH ✓ |
| 2026-07-24 | 30127051147 | Judge validation §6b (mutated) | Corpus RED (divergence at balloon bbox) ✓ |
| 2026-07-24 | 30127357571 | After removing verbose logging | All green ✓ |
| 2026-07-24 | 30129560316 | After chat.res link fix + --textpose dump mode | All green ✓ textpose captured with real emotions ✓ corpus goldens still match ✓ |
| 2026-07-24 | 30131010528 | After first --avatario dump mode (harness build error) | BUILD FAILED: missing externs for EmotionToBytes/BytesToEmotion; sizeof(emFloats) illegal from harness TU ✗ |
| 2026-07-24 | 30131211049 | After avatario externs + kEmFloatsCount fix | All green ✓ avatario captured ✓ corpus goldens still match ✓ |
| 2026-07-24 | 30131585125 | After freezing avatario.golden.json | All green ✓ avatario GOLDEN MATCH ✓ textpose GOLDEN MATCH ✓ |
| 2026-07-24 | 30131970285 | After --avatar-pose dump mode + avatar port | All green ✓ avatar captured ✓ avatario GOLDEN MATCH ✓ |
| 2026-07-24 | 30132205263 | After freezing avatar.golden.json + fixing m_last* writes | All green ✓ avatar GOLDEN MATCH ✓ avatario GOLDEN MATCH ✓ textpose GOLDEN MATCH ✓ |
| 2026-07-27 | 30277135600 | Balloon tails + zoom decision dumps, corpus 011-013 | RED as designed: 10 GOLDEN MISMATCH (added fields), 3 unfrozen, 0 crashed. Produced the full actual.json set to freeze from |
| 2026-07-27 | 30277833899 | After re-freezing all 13 goldens | All green ✓ 13/13 GOLDEN MATCH ✓ glyphs + textpose + avatario + avatar all still MATCH (engine change was observation-only) ✓ |
| 2026-07-27 | 30279392189 | Per-panel-body pose dump, panelWidth knob, CI fixes | RED as designed (13 mismatch, 014 unfrozen). Revealed BOTH new probes still flat: 0/66 bodies with emotion, reduction 1.0 at square 2300 |
| 2026-07-27 | 30280033631 | After bbCooked: 0 on 011 + narrow-tall 014 | RED as designed. **45/45 panel bodies now carry real emotions** (EM_WAVE 1001 for greetings, 3π/2 face for caps→SHOUT) and **014 hits the shrink branch** (reduction 0.6432 / 0.5056 across 4 panels) |

### Round of 2026-07-27: tails, zoom, and three new cases

Closed the two gaps that would have let the balloon/panel unit be ported with
no referee, and the corpus thinness Phase 2's benchmark called for.

- **Tier-3 #5 tail/arrow now pinned.** `m_traj` is NULL after layout because
  the app only builds it in `Draw` (balloon.cpp:1788), so no golden had ever
  contained a tail point. The harness calls `SetBalloonTraj` itself and walks
  `CTraj::m_segs` (`CArc`/`CLine`/`CSpline` via `dynamic_cast`). Safe because
  `SetBalloonTraj` works on a clone of `m_spline` and no layout path reads
  `m_traj`. 313/313 balloons produced one: 313 outline splines + 618 arcs
  (the two `AddArrow` strokes each).
- **Tier-3 #3 zoom decision now pinned.** `zoomFactor` was a local and
  `Establishing()` reads view state, so `panel.cpp` records both at the
  decision site under `ORACLE_HARNESS` (initialised in both ctors, copied by
  the copy ctor, because `Clone()` runs during pagination). 113/267 panels
  zoom, factors 1.30-2.95; `establishing` splits 190 FALSE / 77 TRUE.
- **Verified observation-only:** stripping the new fields from each dump
  reproduces the old golden exactly in all 10 pre-existing cases.
- **Corpus 011-013:** emotion/pose battery, panel-break drivers at
  `panelsWide=3`, and text edge cases. Each uses a seed other than 42.
- **Corpus runner** no longer stops at the first failure - it reports every
  case and fails at the end, so a deliberate re-freeze costs one round instead
  of one per case.

Newly measured gaps (were suspicions, now numbers):

- `reduction` is 1.0 across all 267 panels, so the avatar-overflow shrink
  branch (panel.cpp:784, `sumWidth > m_unitWidth`) is never exercised.
  Two things blocked it. `panelsWide` cannot reach it - in the harness that
  call only sets panels-per-row, because the real narrowing path
  (`CPageView::SetPanelsWide` → `GetProspectivePanelWidth`) needs a window, so
  panel width sat pinned at 4860 for every case. And narrowing width *and*
  height together is self-defeating: bodies scale to
  `maxBodyHeight = m_unitHeight / 1.9` (panel.cpp:755) with their widths
  following, so a square 2300 panel shrank the avatars just as much and still
  reported `reduction` 1.0 (measured, CI 30279392189). Addressed by separate
  `panelWidth`/`panelHeight` inputs (floor `MINUNITPANELWIDTH` = 2300, the
  app's own clamp) and case 014 at 2300x4860 - full-height bodies in a narrow
  frame.
- ~~No `traj` segment is a `line`, so `CBWoodringBox` is never instantiated.~~
  **CLOSED 2026-07-27.** Root cause was the corpus `mode` values: written as
  0/1/2/4, which match neither the `BM_` flags the engine receives
  (defines.h:63 - SAY 1, WHISPER 2, THINK 4, ACTION 8) nor the sequential
  `SM_` values (1..5). `ProcessLine` forwards `uModes` to `AddLine`/
  `MakeBalloon` without `SM2BM`, so `BM_ACTION` was never sent and no box
  balloon was ever built - and 13 of 14 cases were routing through
  `MakeBalloon`'s `default:` "should never happen" branch. All cases now use
  real `BM_` values (verified safe: 12 of 14 diffed only in the echoed `mode`
  field, engine output byte-identical). Case 015 covers `BM_ACTION` and the
  dashed `BM_ACTION|BM_WHISPER`, and pins that `AddLine` tests
  `uModes == BM_ACTION` by exact equality, so 8 breaks the panel and 10 does
  not. Corpus-wide traj census is now 343 splines / 678 arcs / **48 lines**
  across 343 normal + 12 box balloons.

  A second bug surfaced underneath: the traj guard required `m_spline` for
  every balloon, so all 8 box balloons in 015 were skipped even once
  `BM_ACTION` was flowing (`trajSkipped: "no spline or formatInfo"`).
  `CBWoodringBox::SetBalloonTraj` needs only `m_fInfo` - it draws four
  `CLine`s off `m_fInfo->m_bbox`. The `trajSkipped` reason field is what made
  that a one-round diagnosis instead of guesswork.
- **`faceEmotion`/`torsoEmotion` dumped `{0,0}` on every body in every case.**
  Root cause, after two wrong diagnoses recorded here (corpus thinness, then a
  dump defect): **`bbCooked`**. It defaults to 1, and
  `CChatDoc::ProcessLine` only calls `ChatPreSendText` when `!bbCooked`
  (chatdoc.cpp:480) - and that is the sole path running
  `GetEmotionsFromString` → `GetBodyFromEmotion` → `UpdateBody`
  (textpose.cpp:120). So the entire text→emotion→pose pipeline was bypassed
  corpus-wide and every pose in every case was a neutral. Case 011 now sets
  `bbCooked: 0`. The other cases keep 1 deliberately: that is the pre-cooked
  replay path a `.ccc` transcript takes, and it is worth pinning too.

  The two earlier diagnoses were still each half-useful. The neutral
  round-robin is real - `SetFaceNeutral`/`SetTorsoNeutral` (avatar.cpp:419/433)
  scan from `m_lastFace`/`m_lastTorso` for the *next* neutral pose, which is
  why avatar indices kept moving while the emotion stayed `EM_NEUTRAL` - and
  the emotional pose really does live on the panel's own `CBody`, so
  `DumpPanel` now dumps per-body pose (index, emotion, poseID) regardless.
  `avatarStates` is unchanged and still pins the post-message neutral.
- Mid-text `<Brk>` (012 msg8) does not set the page's `newPanel` flag while a
  standalone `<Brk>` (003 msg5) does. Now pinned; the port must match.

### Tier-1 #8 (URL detection) targets dead code (found 2026-07-27)

The plan aims #8 at `urlfind.cpp` (`FIsURL`, `IHexToInteger`) and the
bug-for-bug ledger pins the `||`-vs-`&&` bug at urlfind.cpp:313-326.
**`urlfind.cpp` is compiled into neither modern tree** - it is absent from both
`chat.mak` and `oracle.mak`, and `FIsURL`'s only appearance outside itself is a
vestigial local declaration inside `CChatApp::ProcessShellCommand`
(chat.cpp:1170) that is never called, which is why the link succeeds without
it. So that bug must NOT be reproduced: the code never runs.

The live URL path is `CUrlRec::HrIdentifyUrls`, declared in
`artifacts/inc/urlutil.h:113` and implemented in
`artifacts-modern/core/urlutil.cpp:294`, reached through the three-line
`v2.5-beta-1-modern/urlutil.cpp` shim that `#include`s it. `urlutil.obj` *is*
linked into the harness, and the live caller is `IdentifyURLs`
(format.cpp:1221). #8 should be re-aimed there. Other files absent from
`chat.mak` and therefore dead in the shipped client: `bothdlg`, `cache`,
`cllist`, `dumbwnd`, `guids`, `nmproto`, `script`, `semantic`, `url`,
`urlfind`, `wmini`.

### Watch out when porting the balloon tail (found 2026-07-27)

`CBWoodringNormal::AddArrow` clamps the tail angle with `PI` (balloon.cpp:1505:
`if (fabs(ang) - PI/2.0 > PI/4.0)`) and then feeds the clamped angle through
`cos()` into `xbreak`. The two trees do not share that constant: v1.0-pre's
`vector2d.h:39` has `PI 3.14159`, the v2.5 oracle tree's `vector2d.h:54` has
`PI 3.14159265358979323846`. `port/src/core/numeric.ts` deliberately exports the
low-precision 3.14159 (correct for the v1.0-pre geometry it was written
against), while `port/src/core/emotion.ts` keeps `PI_V25` for the emotion
constants. Since the goldens come from the v2.5 tree, the tail port must use the
v2.5 value: `cos(3*3.14159/4)` and `cos(3*PI_V25/4)` differ by ~1e-6, which
survives multiplication by a TWIPs-scale `heightDelta` and can move `xbreak` by
a unit after `ROUND`.

## Remaining / next-phase work

Phases 0-3 are complete. Per the plan doc, the next phases are:

- **Phase 4 — Port loops** (Loop): port modules in dependency order; corpus read-only.
  Start with `textpose.cpp` (text→emotion rules), then `avatario`/`avatar`,
  then `balloon`/`panel`, then `histent`/`chatdoc`, then `pageview`, then
  `avbfile`/`dib`, then `protsupp`/`ircsock` codecs. Each module should add a
  per-module golden test of the same form as `port/test/engine/spline.test.ts`
  (load corpus `expected.json`, reproduce the dumped struct, assert exact
  equality) so every module gets its own two-agent check.
- **Status as of 2026-07-27:** textpose + avatario + avatar are ported and
  oracle-golden-verified. The next port unit is `balloon.cpp`/`panel.cpp`
  (Tier-3 #4 line breaks + Tier-3 #5/#6 balloon outlines — these are the
  big ones; they pull in the GDI glyph-table consumer + the spline
  consumer both). The oracle side of that unit is now ready: line breaks,
  outline splines, tails, and the zoom decision are all pinned across 13
  cases (see the 2026-07-27 round below). Two things to settle first: the
  `faceEmotion`/`torsoEmotion` dump defect, and whether to add a case that
  overflows a panel so the shrink branch gets a golden.

### Phase 3 outcomes (2026-07-24)

- **Rulebook:** `docs/porting/RULEBOOK.md` — the MFC/GDI→TS/PixiJS idiom map.
  Resolves handoff open question #4 (rulebook location: `docs/porting/`).
  Covers ROUND, value types, overloaded-function suffixes, RNG (LCG),
  glyph-table consumption, geometry/TWIPs, cache policy, integer-width
  semantics, diagnostics, doc model, and the deferred items.
- **Shakedown port:** `port/` (ESM TypeScript + Vitest). Ports `vector2d.cpp`,
  `bbox.cpp`, `spline.cpp`+`splinutl.cpp` — the three purest modules
  (version-identical pure math, no GDI). 75/75 tests pass; `pnpm run typecheck`
  clean.
- **Two-agent check (PASSED):** the C++ oracle (agent A) and TS port (agent B)
  translated `CBeta::ComputeBezpts` independently. All 67 Bézier control
  points of the corpus-001 first balloon match byte-exact. The check caught
  one rulebook ambiguity — the `ROUND` semantics — which is the single most
  important finding: the MSVC `ROUND` macro behaves as `Math.floor(fp + 0.5)`
  (round-half-up), NOT `Math.trunc(fp + 0.5)` as the C source reads. With
  `trunc`, 0/67 bezpts match; with `floor`, 67/67 match. The oracle is the
  referee. See RULEBOOK §1.
- **Bugs pinned (bug-for-bug, plan doc §7):**
  - `bbox.cpp:76` `bbox_within_bbox` reads `bbox2.bottom` (should be
    `bbox1.bottom`) — reproduced in `port/src/engine/bbox.ts`, marked
    `BUG(port)`, tested.
  - `splinutl.cpp:204` `int_bezier_nearest_point` uses `(int)` truncation
    (not `ROUND`) despite the `// should round` comment — reproduced as
    `Math.trunc`, marked `BUG(port)`.

### Tier-1 stateless codecs DONE (2026-07-27, CI 30285677005)

`--codecs` dumps #6, #7, #8, #9 and #12 into `oracle/codecs/codecs.golden.json`.
What the goldens pinned, beyond the plain round trips:

- **#9:** `IndexToByte(i) = i + '0'`, `ByteToIndex(b) = b - '0'` - the `value+'0'`
  encoding the plan predicted for UDI. `SM2BM` shows **`SM_SHOUT` (4) has no BM
  equivalent** and falls through to `BM_SAY`; `BM2SM` shows `BM_ACTION`
  dominating every combination containing it.
- **#7:** the `^k33` -> `^k0133` hack (format.cpp:502) emits
  `[0x03, '0', '1']` when the next char is a digit and bare `[0x03]` otherwise.
  **The colour palette is 16 entries, not 32**: `GetRBGColor` returns black for
  every code 16..31, and `GetColorCode(black) = 1`, so the mapping is not
  injective. The `+16` in `nFillFormatting` is a *wire* offset that keeps the
  emitted decimal always two digits - a decoder must subtract 16 before the
  palette lookup, and a port that forwards the wire value would render
  everything black.
- **#6:** `lastString` carries the trailing `":..."` payload (the message text
  for PRIVMSG) and the remainder once `nArgs` hits `MAXARGS` - the spill keeps
  the leading space, e.g. `' j k l m n o p'`. Numerics parse into `uCode`
  (001 -> 1). A prefix with no following space is **outside the contract**:
  `ParseIt` guards it with `ASSERT` alone (ircsock.cpp:159), which vanishes in
  release, and the next line does `NULL - szMessage`. Malformed input off the
  wire reaches this in the shipped client too.
- **#8:** re-aimed at the live `HrIdentifyUrls`. A trailing period and a closing
  paren are both excluded from the match; `mic://`, `mailto:` and `ftp://` are
  recognised; **`www.example.com` with no scheme is not detected at all**, since
  the scan is colon-driven.
- **#12:** six appends into a four-slot ring keeps `three`..`six`; walking back
  past the oldest **clamps** (repeats `three`) rather than wrapping, and walking
  past the newest yields empty strings.

Two engine habits worth carrying into the RULEBOOK: functions taking `const`
strings write through them (`HrIdentifyUrls` stamps a terminator over the
scheme's colon and restores it; `ParseIt` casts away const via `UnConst`), and
preconditions live in `ASSERT`s that do not exist in the shipped build.

### Tier-1 #10 (.ccc transcript codec) DONE (2026-07-27, CI 30287420118)

`--ccc` freezes byte-exact `WriteSelf` output for all nine keywords plus a
parse-then-serialise pass, in `oracle/ccc/ccc.golden.json`. Entries are built
with their live ctors and round-tripped, so the golden pins the format rather
than my transcription of it.

**9 of 11 probes round-trip byte-exactly.** The two that do not are correct by
design, and that is the finding - a port test asserting byte identity on
re-serialise would be wrong for both:

- **`changeavatar` is not round-trip stable.** `WriteSelf` branches on `m_avID`:
  a live entry (`m_avID == 0`) writes the requested `m_avName`/`m_avURL`, but a
  *parsed* entry has `m_avID` set by `GetAvatar3` in the parse ctor, so
  `WriteSelf` emits `pAv->OriginalName()` and `pAv->Url()` - the resolved
  avatar's values - and discards what the transcript recorded. A locally loaded
  avatar has no URL, so re-serialising drops it. The comment states the intent:
  "we want to restore the same visuals that the user saw if possible".
- **`starthistory` substitutes a nick.** The parse ctor loads
  `IDS_DEFAULT_NICK` ("Anonymous") when the nick field is empty, while the live
  ctor never sets `m_name`, so write and read disagree.

Also pinned:

- **The seed really is absent.** `StartHistoryEntry` reads keyword, nick, avName
  and title - there is no seed field in either direction. Plan section 4.3 is
  now confirmed from both sides: a `.ccc` cannot reproduce a comic, and the
  harness must keep injecting the seed.
- `comicchar` writes the literal string `(null)`: `ComicCharacterEntry(pui)`
  passes NULL info to `GetInfoEntry` by design and MSVC's `printf` renders it
  that way. Shipped behaviour, so the port must emit the same literal.
- The record terminator is not part of a record. Feeding the CRLF back into a
  parse ctor makes it part of the final field and `QuoteReturns` re-quotes it as
  a literal `\r\n`, which is what made all eleven probes look broken at first.

Outstanding oracle-side TODOs (not blocking Phase 4, but improve coverage):

- Tier-1 scoreboard: #1 textpose, #2 avatar pose, #3 avatario, #6 IRC line
  parse, #7 formatting codec, #8 URL detection, #9 mode maps, #10 `.ccc`
  transcript codec, #11 seeded RNG, #12 `CDosKey` ring — all DONE. Remaining:
  **#4** (UDI annotation codec) and **#5** (identity comments), both of which
  need a document plus a `CUserInfo`; #4's encode side additionally needs
  `bInsertAnnotations` un-`static`'d under `ORACLE_HARNESS` and a `MyAvatar()`
  self-avatar. That would be the first guard to change a symbol's linkage
  rather than just add an observable, so it wants a deliberate decision.
- **#13** (CJK jis2sjis/intl.c) deferred — a scope question, not a build task.
- Tier-2: manifests + pixel CRCs DONE (`--avb`, see below). Remaining Tier-2
  sub-items are small and listed in that section.
- Pixel-level (Tier-3 #8 raster hash) goldens deferred until Tiers 1-7 green.
- Corpus is at 15 cases (001-015). The emotion gap called out here originally is
  closed by 011 (needs `bbCooked: 0`), the overflow-shrink branch by 014, and
  box balloons by 015.

## Tier-2: `.avb`/`.bgb` manifests + decoded-pixel CRCs (2026-07-27)

`--avb <outDir> [artDir]` writes one manifest per ComicArt asset plus an
`index.json`, frozen as `oracle/avb/<stem>.golden.json`.

Sharded per asset rather than one dump. A single golden covering 32 assets and
~1500 poses would be multi-megabyte and its diff would name nothing useful;
per-asset, a mismatch names the asset that moved.

Each manifest has two deliberately separate layers:

- **Pre-load** — per pose, the three `(offset, format, paletteType)` triples
  straight out of the pose record. No decode involved, so these stay meaningful
  even when an image fails to decode.
- **Post-load** — per image slot, the `BITMAPINFOHEADER` scalars, the colour
  table CRC, and a CRC32 over the pixel bytes.

Both are in one dump because the load call is the same; the split is in the JSON,
not in the CI round. A per-pose exception guard degrades a decode fault to a
`loadStatus` field instead of killing the run, so one bad pose still yields a
freezable dump naming the pose that faulted (the lesson `--codecs` cost two CI
rounds to learn).

`--avb` runs on `AfxWinInit` alone — no DC, no fonts, no emotion table, no avatar
registry. `avbfile.cpp` and `dib.cpp` reference `theApp` only for the `#include`,
never for state, so the dump cannot be perturbed by anything outside the two
files under test. Worth preserving.

Decisions worth knowing:

- **CRCs are hex strings, not numbers.** ojson emits integers with `%ld`, so any
  CRC with the top bit set would land in the golden as a negative number.
- **`m_avatarID` is deliberately not dumped.** It is assigned by the registry at
  load time, so including it would make the golden depend on load order.
- **Poses are walked by array index, not by `GetPoseFromID`.** Index `i` *is*
  poseID `i+1` (`m_arrPoses[nID - 1]`, avatar.cpp:135), and the public
  `GetPoseFromID` overloads on the derived classes fall back to *searching* for a
  nearby pose on a miss — which would silently substitute a different pose. The
  dump calls `CPose::Load` itself so a miss is reported as a miss.
- **Loaded via `CAvatarX::LoadAvatar`, not the engine's `LoadAvatar(name)`.**
  `LoadAvatarInfo` derives its path from `theApp.GetAvatarDir()` and then calls
  `SetNewName`, which overwrites `m_name` with the *filename* — exactly the field
  a format manifest wants to pin from the `AK_NAME` record.
- **`AK_COPYRIGHT` stores a literal two-character `\n` escape, not a newline.**
  bolo's is `Copyright © 1996, 1997, 1998 Microsoft Corporation\nJim Woodring`
  where `\n` is backslash-then-n in the file bytes. The codec passes it through
  verbatim; interpreting the escape is a *rendering* concern, so a port that
  un-escapes on read will not match the golden. The `©` is a single byte 0xA9
  (CP-1252), which is why the goldens carry it as `©` per ojson's
  byte-oriented string contract.
- **`biSizeImage` cannot be trusted as the pixel length.** It is documented as
  "may be 0" for uncompressed DIBs and `dib.cpp` does set it to 0 on the paths
  that build headers by hand (dib.cpp:295, 377). For `BI_RGB` the length is
  `StorageWidth() * abs(biHeight)`; for the RLE compressions `biSizeImage` IS the
  length and the stride formula does not apply. Both are recorded so a port that
  picks the wrong one shows up as a length mismatch, not a silent CRC drift.

### dib.h declares nine methods that dib.cpp does not define

First `--avb` CI round failed to link with exactly one error:

    oracleharness.obj : error LNK2019: unresolved external symbol
    "public: int __thiscall CDIB::GetNumClrEntries(void)"

`CDIB::GetNumClrEntries` is declared in dib.h:30 — **outside** that header's own
`#if 0` region — but its definition sits **inside** a 546-line `#if 0` in
dib.cpp (260-806). So it is a declaration with no definition, and the harness was
its first caller in the whole tree.

The same `#if 0` swallows eight more definitions whose declarations are likewise
visible: `Create(int,int)`, `Load(CFile*)`, `Load(FILE*)`, `Load(LPCSTR)`,
`MapColorsToPalette`, `GetPixelAddress`, `GetRect`, `CopyBits`.

Fixed by calling the free function `NumDIBColorEntries` (dib.cpp:70, compiled)
which is all the method wrapped anyway.

**Port note:** treat dib.h as an unreliable inventory. Nine of its declarations
are unbacked, so "declared in the header" is not evidence a method exists, let
alone that the engine uses it. Port `CDIB` from what dib.cpp actually compiles.

### Port trap: the record tables are not initialised on construction

`CAvatarX::Initialize` (avatar.cpp:886) covers only base-class members.
`CAvatarComplex()` sets just `m_lastFace`/`m_lastTorso` and `CAvatarSimple()`
just `m_lastBody`, so **`fRec`, `bRec`, `nFaces`, `nTorsos` and `m_nBodies` are
indeterminate** until `LoadFaceRecs`/`LoadTorsoRecs`/`LoadBodyRecs` writes the
pointer and the count as a pair (avbfile.cpp:1077, 1181, 1255).

An `.avb` missing its `AK_NFACES`/`AK_NTORSOS`/`AK_NBODIES` record therefore
leaves *both* garbage — so a NULL check on the pointer is not enough, and
iterating `count` entries reads out of bounds. Every shipped asset has the
records, which is why the engine gets away with it.

**The port must zero-initialise these fields rather than mirror the C++
constructors.** The dump carries a plausibility guard (`RecCountPlausible`) that
emits `faceRecsUnreadable`/`torsoRecsUnreadable`/`bodyRecsUnreadable` instead of
hashing stack garbage; it should never fire on the shipped corpus.

### Measured: what the frozen manifests DO cover

From the 33 frozen goldens (run 30297033114, all 32 assets `loadStatus: ok`,
all 552 poses `ok`):

| | |
| --- | --- |
| assets | 32 (18 complex + 7 simple avatars, 7 backdrops) |
| poses | 552 |
| decoded image slots hashed | 1612 |
| decoded pixel bytes hashed | 13,780,864 (9x the 1.5 MB on disk) |
| stored image records | 745 |

1612 decoded slots from 745 stored images is the **2-bpp expansion working**: one
`AIP_MASKEDMONO`/`AIP_DUALMASK` source becomes two or three 1-bpp DIBs via
`ConvertFromMaskedMono`/`ConvertFromDualMask`. The decoded bpp histogram agrees —
1387 slots at 1bpp against 136 at 4bpp and 89 at 8bpp.

**143 repeated image offsets within an avatar**, so the ditto-optimization
(equal `dwImageOffset` reuses a pose, avbfile.cpp:1093+) is genuinely exercised.

Palette types across the 745 records: `AIP_MASKEDMONO` 334, `AIP_LOCALPALETTE`
218, `AIP_DUALMASK` 192, `AIP_MONOCHROME` **1**.

### Measured: what the shipped corpus does NOT cover

Four more gaps beyond the format-version ones below, all from the frozen
manifests rather than guessed:

- **`AIF_DIB` is never used.** All 745 image records are `AIF_LZDEFLATE`, so
  `CAvatarFileDIBImage::Read` has no golden — only the zlib path is exercised.
  The plan lists both.
- **`AIP_GLOBALPALETTE` is never used, and no avatar has a non-empty global
  palette** (`m_nColorCount == 0` for all 25). So the `AK_COLORPALETTE` record and
  `GetProperPalette`'s global branch are untested; every image carries its own
  local palette instead.
- **`AIP_NOPALETTE` is never used** in an avatar pose record.
- **`AIP_MONOCHROME` appears exactly once** in the whole corpus — pinned, but a
  single sample is thin evidence for that branch.



From the asset headers directly (25 `.avb` + 7 `.bgb`, all in
`v2.5-beta-1-modern/ComicArt`):

| Fact | Count |
| --- | --- |
| magic `0x8181` (`AF_MAGICNUM_NEW`), version 2 | 32 of 32 |
| magic `0x81` (`AF_MAGICNUM`, old format) | **0** |
| `AT_COMPLEX` avatars | 18 |
| `AT_SIMPLE` avatars | 7 |
| `AT_BACKDROP` | 7 |
| `.bgb` via the `0x8181` container branch | 7 |
| `.bgb` via the plain-DIB `'BM'` branch (avbfile.cpp:1669) | **0** |
| `.bmp` backdrops | **0** |

So three format branches have **no possible golden from this corpus**: the old
`0x81` magic, the old record tags (`AK_NFACES`=4 / `AK_NTORSOS`=5 / `AK_NBODIES`=9
and the `olddata` struct layouts with their padding bytes), and the `'BM'`
backdrop branch. They exist for backwards compatibility with files that no longer
ship. The port should either omit them or reproduce them knowing they are
untested — but not believe a green Tier-2 covers them.

**Asset-count discrepancy with the plan.** TEST-ORACLE-PLAN.md §Tier 2 says
"45 `.avb` + 9 `.bgb` + 2 `.bmp` in `comicart/` + `artpack1/` (~5 MB)". This repo
has **25 `.avb` + 7 `.bgb` + 0 `.bmp`, 1.5 MB**, and no `artpack1/` at all. The
plan's free cross-check — "four characters (bolo, cro, denise, lynnea) exist in
two independent encodings" — is therefore **not available**: each of those four
appears exactly once here. Plan corrected to match the tree.

### Remaining Tier-2 sub-items

- **Backdrop world constants.** `SetBackDropAux` (backdrop.cpp:91-95) hardcodes
  `xdim = ydim = 315`, world `(0, 0, 4860, -4860)`, `normHeight = 100` for every
  backdrop — the metadata is *not* in the file, as the plan notes. Pinning it
  through `BackDropArtFromBackID` -> `CBackDropArt::m_worldCoords` needs the
  backdrop registry set up (`SetBackDropAux` then `GetBackDropArtFromID`), i.e.
  `InitHarness`-level state, so it does not belong in `--avb`'s deliberately
  minimal mode. Own dump, small.

  Established while scoping it: **the printer-side backdrop array is entirely
  vestigial.** `backRecP` is only ever `SetSize`d and `Add(NULL)`'d
  (backdrop.cpp:170-171, 327) — nothing ever puts a real `BDFileRec` in it, since
  `SetBackDropAux` adds only to `backRecS`. `NotifyDownloadedBackdrop` does
  iterate it (backdrop.cpp:380) but every element is NULL so the
  `if (pRec && ...)` guard skips them all. And the one place that would index it,
  `BackDropArtFromBackID`, is guarded by `(SCREENPRINT || toScreen)` where
  `#define SCREENPRINT 1` (backdrop.cpp:239) makes the condition a constant true,
  so the `backRecP[backID]` branch is unreachable. The `BDFileRec backRecP[]`
  table at backdrop.cpp:214 is inside the `#if 0` block.

  **Port guidance: implement the screen array only; do not port `backRecP` /
  `backMapP`.** Note this is not a working print path that merely happens to be
  disabled — if `SCREENPRINT` were ever set to 0, `backRecP[backID]` would index
  past the array's single NULL element. Same category as the `urlfind.cpp`
  `IHexToInteger` bug: dead code whose defects must not be faithfully reproduced.
- **Record-tag inventory.** The manifest pins the *results* of the tag walk but
  not the tag sequence itself, so it cannot say which of `AK_*` a given file
  exercises. Getting that means an `ORACLE_HARNESS` trace in
  `HandleLoadTag` — kept out of this round so a red CI round would have one
  cause, not two.
- **Pre-conversion `biCompression` is unobservable.** `CAvatarDIB::Load` ends with
  an unconditional `ConvertToNonRLE()` (avbfile.cpp:462), so every DIB the dump
  can reach is already `BI_RGB` and the manifest cannot say whether an asset was
  RLE4/RLE8 on disk. The RLE expanders *are* covered — the `pixelCrc32` is taken
  post-expansion, so a port that skips expansion diverges — but **whether any
  shipped asset exercises them at all is currently unmeasured**, and the images
  are zlib-framed inside the `.avb` so it cannot be checked from outside either.
  The only hint in the manifest is indirect: the expanders set
  `biSizeImage = newSize` (dib.cpp:931, 1012) whereas a file-native `BI_RGB` DIB
  may carry 0. Wants the same `ORACLE_HARNESS` hook as the tag inventory, in the
  same round.
- **`.ccr` locator parse** (setupdlg.cpp:892-980) — untouched.

## Branches (as of 2026-07-27)

`main` was renamed to `staging`, and the model is now three branches:

| Branch | Default | CI | Role |
| --- | --- | --- | --- |
| `dev` | yes | **none** | Day-to-day work. Absent from every workflow trigger, so pushes here never spend CI. No PRs. |
| `staging` | no | oracle + build + port tests on push | Promotion target. This is where the goldens actually get gated. |
| `production` | no | same three workflows on push | Release branch. |

The workflow files *are* present on `dev` — they simply don't name it in their
`push:` branch filters. Deleting them on `dev` instead would mean every
`dev -> staging` merge also deleted staging's CI, which is why it wasn't done
that way.

Because `dev` runs nothing, the local commands are the only feedback while
working there:

    cd port   && pnpm install --frozen-lockfile && pnpm run typecheck && CI=true pnpm test
    cd oracle && pnpm install --frozen-lockfile   # then: node scripts/validate.mjs --corpus corpus

The C++ oracle harness itself needs Windows + MSVC, so its goldens can only be
re-verified by pushing to `staging` (or dispatching the workflow manually).
That is the one gap the no-CI-on-dev rule leaves: a change to
`v2.5-beta-1-modern/**` is unverified until it reaches `staging`.