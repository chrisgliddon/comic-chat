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
  `panelsWide` cannot reach it: in the harness that call only sets
  panels-per-row, because the real narrowing path
  (`CPageView::SetPanelsWide` → `GetProspectivePanelWidth`) needs a window, so
  panel width sat pinned at 4860 for every case. Addressed by a `panelWidth`
  corpus input (floor `MINUNITPANELWIDTH` = 2300, the same clamp the app
  applies) plus case 014.
- No `traj` segment is a `line`, so `CBWoodringBox` (four-`CLine` box balloon)
  is never instantiated by the corpus.
- **`faceEmotion`/`torsoEmotion` dump as `{0,0}` on every body in every case,
  including 011.** First recorded here as corpus thinness, then as a dump
  defect; both were wrong. `CAvatarX::GetEmotions` reads the *pose record's*
  catalogued emotion, and by dump time `ResetAvatar` has called `SetNeutral`.
  `SetFaceNeutral`/`SetTorsoNeutral` (avatar.cpp:419/433) scan round-robin from
  `m_lastFace`/`m_lastTorso` for the *next* neutral pose, so the avatar's
  indices legitimately keep moving while its emotion stays `EM_NEUTRAL` (0.0).
  The `avatarStates` dump is therefore faithful - it pins the post-message
  neutral, which is itself load-bearing state - but it was the only place pose
  data appeared, and the *emotional* pose lives on the panel's own `CBody`.
  Fixed by dumping per-panel-body pose (index, emotion, poseID) in `DumpPanel`;
  `avatarStates` is unchanged.
- Mid-text `<Brk>` (012 msg8) does not set the page's `newPanel` flag while a
  standalone `<Brk>` (003 msg5) does. Now pinned; the port must match.

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

Outstanding oracle-side TODOs (not blocking Phase 4, but improve coverage):

- Tier-2 (`.avb` manifests + decoded-pixel CRCs) subcommand not yet implemented.
- Tier-1 unit-level dumps: textpose (DONE --textpose), avatario
  (DONE --avatario), avatar pose (DONE --avatar-pose). Remaining Tier-1:
  UDI annotation codec (#4), identity comments (#5), IRC line parse (#6),
  formatting codec (#7), URL detection (#8), mode maps (#9), .ccc
  transcript codec (#10).
- CJK path (jis2sjis/intl.c) deferred — open question whether in MVP scope.
- Pixel-level (Tier-3 #8 raster hash) goldens deferred until Tiers 1-7 green.
- More corpus cases could be added for deeper Tier-1/3 coverage (the 10 cases
  are a happy-path baseline; the completeness critic in `oracle/scripts/critic.mjs`
  can identify gaps). The current 10 corpus cases don't exercise non-zero
  emotion dumps — consider adding cases that trigger SHOUT, WAVE, etc. to
  give the avatar port a real corpus golden (open question #4 from the
  handoff).

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