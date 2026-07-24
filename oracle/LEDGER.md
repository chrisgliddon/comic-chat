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

## Remaining / next-phase work

Phases 0-3 are complete. Per the plan doc, the next phases are:

- **Phase 4 — Port loops** (Loop): port modules in dependency order; corpus read-only.
  Start with `textpose.cpp` (text→emotion rules), then `avatario`/`avatar`,
  then `balloon`/`panel`, then `histent`/`chatdoc`, then `pageview`, then
  `avbfile`/`dib`, then `protsupp`/`ircsock` codecs. Each module should add a
  per-module golden test of the same form as `port/test/engine/spline.test.ts`
  (load corpus `expected.json`, reproduce the dumped struct, assert exact
  equality) so every module gets its own two-agent check.
- **Phase 5 — Differential** (Loop): both runners over full corpus; drive diff to zero.
- **Phase 6 — Idiomize + web shell**: PixiJS rendering, WebSocket IRC bridge, UI.

### Phase 3 outcomes (2026-07-24)

- **Rulebook:** `docs/porting/RULEBOOK.md` — the MFC/GDI→TS/PixiJS idiom map.
  Resolves handoff open question #4 (rulebook location: `docs/porting/`).
  Covers ROUND, value types, overloaded-function suffixes, RNG (LCG),
  glyph-table consumption, geometry/TWIPs, cache policy, integer-width
  semantics, diagnostics, doc model, and the deferred items.
- **Shakedown port:** `port/` (ESM TypeScript + Vitest). Ports `vector2d.cpp`,
  `bbox.cpp`, `spline.cpp`+`splinutl.cpp` — the three purest modules
  (version-identical pure math, no GDI). 75/75 tests pass; `npm run typecheck`
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
- Tier-1 unit-level dumps (GetEmotionsFromString table, emotion quantization)
  not yet implemented as standalone dump modes.
- CJK path (jis2sjis/intl.c) deferred — open question whether in MVP scope.
- Pixel-level (Tier-3 #8 raster hash) goldens deferred until Tiers 1-7 green.
- More corpus cases could be added for deeper Tier-1/3 coverage (the 10 cases
  are a happy-path baseline; the completeness critic in `oracle/scripts/critic.mjs`
  can identify gaps).

## Merging

`oracle/phase-0` is ready to merge to `main` (no PR needed per user). The
branch contains all Phase 0-2 work + frozen goldens + CI that guards them.

Phase 3 work (rulebook + `port/` shakedown) is uncommitted on `main` as of
this LEDGER update. It is TS-only (no C++ engine or golden changes) so no CI
round is required to validate it; `npm test` and `npm run typecheck` in
`port/` are the verification commands. Commit when ready.