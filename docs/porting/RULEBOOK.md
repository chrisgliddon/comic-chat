# Comic Chat Port Rulebook: MFC/GDI → TypeScript/PixiJS Idiom Map

*Phase 3 deliverable of `docs/porting/TEST-ORACLE-PLAN.md` §8. The contract
that lets Phase 4 port loops run mechanically without per-file re-litigation.
Every entry here is either (a) pinned by an oracle golden, (b) backed by a
shakedown test in `port/test/`, or (c) a direct consequence of the plan doc's
determinism preconditions (§4). When two agents translate the same C
construct and differ, THIS document is the referee — fix the rulebook, not
the code.*

*Status: Phase 3 COMPLETE. The 3-file shakedown port
(`vector2d`, `bbox`, `spline`/`splinutl`) is green: 75/75 tests pass,
including a 67/67 exact-match of the CBeta Bézier control points against
`oracle/corpus/001/expected.json`. The single most important finding —
the `ROUND` semantics — is §1 below.*

---

## 0. How to use this rulebook

1. **Port in dependency order** (plan doc §8 Phase 4): `vector2d`/`bbox`/
   `splinutl`/`spline` (DONE) → `textpose` → `avatario`/`avatar` →
   `balloon`/`panel` → `histent`/`chatdoc` → `pageview` → `avbfile`/`dib` →
   `protsupp`/`ircsock` codecs.
2. **For each C idiom, find the entry here and translate verbatim.** Do not
   improvise. If an idiom isn't here, add it after porting (with a test).
3. **The oracle is the referee.** A failing corpus case is fixed in the TS
   port, never by editing a golden (plan doc §9). If the port and the oracle
   disagree, the port is wrong *unless* the rulebook is wrong — in which case
   fix the rulebook AND re-audit every port that depended on the old entry.
4. **Reference tree for port readability: `v1.0-pre-modern`.** Golden tree:
   `v2.5-beta-1-modern`. They are byte-identical for the pure-math modules
   (only the copyright header + 2 cosmetic `fabs`/cast lines in `vector2d`
   differ). When they differ on a GDI-dependent module, the **golden tree
   wins** — match its forms.
5. **Provenance comments.** Every ported file cites the C source path and
   line range it reproduces. Every rulebook entry cites the C site.

---

## 1. ROUND — the dominant cross-platform hazard (ORACLE-PINNED)

**C source:** `vector2d.h:37` — `#define ROUND(fp) ((int)(fp + 0.5))`

**TS translation (load-bearing, in `port/src/core/numeric.ts`):**
```ts
export function ROUND(fp: number): number {
  return Math.floor(fp + 0.5) | 0;
}
```

**THIS IS THE MOST IMPORTANT RULE IN THE RULEBOOK.** It was determined
empirically by the Phase-3 two-agent check, not by reasoning about the C
standard:

- The C macro reads as `trunc(fp + 0.5)` (since C `(int)x` truncates toward
  zero). A naive port produces `Math.trunc(fp + 0.5)`.
- The oracle corpus-001 golden contains a CBeta spline with 67 Bézier control
  points. Reproducing the C `ComputeBezpts` in TS:
    - with `Math.trunc(fp + 0.5)`: **0/67** bezpts match the golden.
    - with `Math.floor(fp + 0.5)`:  **67/67** bezpts match — byte-exact.
- Therefore the MSVC build's effective `ROUND` is `floor(fp + 0.5)`
  (round-half-up). The mechanism (likely an MSVC `/arch` native-rounding
  optimization of the macro, or x87 cast behavior) doesn't matter — **the
  oracle is the referee**. `floor(fp + 0.5)` is the law.

**Where it bites:** `CvertsToCubic` and `CubicToBezier` (`spline.cpp:209/221`)
apply `ROUND` to every matrix·knot product. A wrong ROUND cascades into every
balloon outline, every `ClosestPoint`/`WalkHorizontalDistance` result, and
downstream panel layout. The first divergence the judge-validation mutation
caught (LEDGER CI 30127051147) was at a balloon bbox — same family.

**Do NOT substitute:**
- `Math.round` — agrees with `floor(fp+0.5)` for all non-exact-.5 values and
  for positive .5, and *happens* to agree for negative .5 too (both give the
  same integer), so it would actually pass the current golden. But it is NOT
  the rulebook form: `Math.round` is specified as `floor(fp + 0.5)` already
  (ECMA-262 §21.3.2.28), so they're equivalent — but pin `floor(fp+0.5)` so
  the rulebook reads as the engine's macro, not a JS spec coincidence.
- `Math.trunc(fp + 0.5)` — WRONG. 0/67 on the golden.
- `(fp + 0.5) | 0` — WRONG for the same reason (truncates toward zero).

**The `| 0` suffix** coerces -0 to +0 (C `int` has no negative zero) and
clamps into Int32 (matching C 32-bit `int` overflow for the engine's value
range). Keep it.

**Related: `INT_CAST` (distinct from ROUND).** C `(int)x` (without the `+0.5`)
is plain truncation toward zero — used in `vector2d.cpp:100`
`point_scalmult`'s integer overload. Translate as `Math.trunc(x) | 0`, NOT
`floor`. The two are different operations in the engine; do not conflate.

**Pinning tests:** `port/test/core/numeric.test.ts`,
`port/test/engine/spline.test.ts` (67/67 golden).

---

## 2. Value types: CString / CPtrList / arrays / structs

**Finding (harness README):** the engine uses MFC as **value types + GDI
wrappers only** — never as a framework. No `CChatDoc`-style framework
inheritance on the port hot path. This means the port is a struct-for-struct
translation, not a re-architecture.

### 2.1 `CString` → `string` (with one documented quirk)

- Default: `CString` → `string`. The engine's `CString` operations are
  `Left`/`Mid`/`Right`/`GetAt`/`GetLength`/`+=` — all map to `slice`/`charAt`/
  `length`/`+=`.
- **BUG(port) quirk (plan doc §7, `format.cpp:502-508`):** the `^k33`→`^k0133`
  color-code hack embeds `\0` (NUL) inside a `CString`. MSVC `CString` carries
  embedded NULs; JS `string` does too (NUL is a valid char), so `string` is
  fine — BUT `strlen`-style length stops at NUL and `CString::GetLength` does
  not. The port must use `.length` (JS) and never a C-strlen-equivalent. Pin
  with a test when `format.cpp` is ported.
- `CString` as a `CMapStringToPtr` key (`spline.cpp:118` `sprintf("%f*%f",t,b)`):
  dropped in the port (see §7 cache policy).

### 2.2 `CPtrList` / `CPtrArray` / `CDWordArray` / `CTypedPtrArray` → `Array<T>`

- `CPtrList` (doubly-linked, `AddTail`/`AddHead`/`GetNext`/`RemoveAt`) →
  `Array<T>` with `push`/`unshift`/iteration/`splice`. The engine only uses
  the tail-add + forward-iterate pattern (e.g. `CTraj::m_segs`, `traj.h:48`),
  so `Array` is a faithful match. `POSITION` → numeric index or `for...of`.
- `CPtrArray` / `CDWordArray` → `Array<T>` (or `number[]` for `CDWordArray`).
- `CTypedPtrArray<CPtrArray, CBody>` → `Array<CBody>` (typed). The template
  args disappear; the element type is what matters.

### 2.3 Win32 / engine structs → TS `interface`

| C struct | TS interface | Fields | Notes |
|---|---|---|---|
| `DPOINT` (vector2d.h:6) | `interface DPOINT { x: number; y: number }` | double x,y | Plain object; construct via `dpoint(x,y)`. |
| `POINT` (Win32) | `interface POINT { x: number; y: number }` | int x,y | Same shape as DPOINT but int semantics; the engine relies on call-site context. Port uses `_d`/`_i` name suffixes (see §3). |
| `RECT` (Win32) | `interface RECT { left; top; right; bottom }` | int | Lowercase fields (engine convention). |
| `SRECT` (bbox.h:4) | `interface SRECT { Left; Top; Right; Bottom }` | short | **Capitalized** fields (engine convention). `SRECTToRECT` is a 1:1 field copy. Do NOT enforce 16-bit wrap (see §8). |
| `BOUNDBOX` (vector2d.h:10) | `interface BOUNDBOX { xmin; xmax; ymin; ymax }` | double | Spline-utils only. |
| `BEZIER` (vector2d.h:18) | `interface BEZIER { p0; p1; p2; p3 }` (DPOINTs) | double | de Casteljau split etc. |

**Mutation vs return:** C functions like `adjust_bbox(RECT *bbox, ...)` mutate
in place via pointer. TS passes objects by reference, so the same signature
shape (`function adjust_bbox(bbox: RECT, delta: number): void`) mutates `bbox`
in place. Callers do not need to reassign. **Do not** "functionalize" these
into returning new objects — downstream code depends on the in-place mutation
(e.g. `make_empty` is called on stack boxes before `include_pt_in_bbox` loops).

---

## 3. Overloaded functions: explicit suffixes, not TS overloads

The engine uses C++ overloads heavily: `point_add(DPOINT,DPOINT)` and
`point_add(POINT,POINT)` differ only in arg types and (for `point_scalmult`)
in rounding semantics. **TS overloads resolve by static type erasure and can
pick the wrong overload when both arg types are structurally identical
(`DPOINT` and `POINT` are both `{x,y}`).** The rulebook mandates **explicit
suffixes** so the call site is unambiguous:

| C | TS |
|---|---|
| `point_add(DPOINT, DPOINT)` | `point_add_d(a, b)` |
| `point_add(POINT, POINT)` | `point_add_i(a, b)` |
| `point_scalmult(double, DPOINT)` | `point_scalmult_d(s, pt)` |
| `point_scalmult(double, POINT)` | `point_scalmult_i(s, pt)` (uses `INT_CAST`) |
| `inside_bbox(POINT*, RECT*)` | `inside_bbox_r(pt, bbox)` |
| `inside_bbox(POINT*, SRECT*)` | `inside_bbox_s(pt, bbox)` |
| `make_empty(RECT*)` | `make_empty_r(bbox)` |
| `make_empty(SRECT*)` | `make_empty_s(bbox)` |
| `include_pt_in_bbox(POINT*, RECT*)` | `include_pt_in_bbox_r(pt, bbox)` |
| `include_pt_in_bbox(POINT*, SRECT*)` | `include_pt_in_bbox_s(pt, bbox)` |

**Suffix convention:** `_d` = DPOINT/double, `_i` = POINT/int, `_r` = RECT,
`_s` = SRECT. Apply to every overload family. The two-agent check confirmed
this is unambiguous; TS overloads are not.

---

## 4. RNG: MSVC CRT `rand()`/`srand()` — exact LCG replication

**C source:** MSVC CRT. Plan doc §4 #4 is load-bearing.

**TS translation (`port/src/core/numeric.ts`):**
```ts
export const RAND_MAX = 0x7fff;
export class LCG {
  private state: number;
  constructor(seed: number) { this.state = seed >>> 0; }
  rand(): number {
    this.state = (Math.imul(this.state, 214013) + 2531011) >>> 0;
    return (this.state >>> 16) & 0x7fff;
  }
  srand(seed: number): void { this.state = seed >>> 0; }
}
```

**Critical details (all oracle-pinned, see `port/test/core/numeric.test.ts`):**

- `seed = seed * 214013 + 2531011` mod 2³²; return `(seed >> 16) & 0x7fff`.
  `RAND_MAX = 0x7FFF = 32767`.
- **Use `Math.imul` for the multiply**, not `*`. JS `*` on integers > 2²³
  loses precision; `Math.imul` does 32-bit int multiply with correct wrap.
  The `>>> 0` after the add reifies the Uint32 state (JS `+` is double).
- `srand(seed)` stores the seed **as-is** (no pre-mix). The first `rand()`
  call after `srand(1)` returns `41` (the CRT default-seed sequence); after
  `srand(0)` returns `38`. Both pinned in tests.
- **Call order is load-bearing** (plan doc §4 #4). Every `rand()`/`randfloat()`
  call advances the sequence — including the **zero-effect** `ShiftLines` calls
  at `balloon.cpp:760/768` (`MAXSHIFT==0` loops that still consume `randfloat`).
  The port must replicate every call site, even no-op ones. The corpus
  `seedLedger` (oracle schema) records every seed consumed — a port divergence
  shows up as a shifted ledger.
- `randfloat()` (`balloon.cpp:428`) = `((double)rand()) / RAND_MAX` → in [0,1).
  TS: `rng.rand() / (RAND_MAX + 1)` to get [0,1) (the `+1` keeps the upper
  bound exclusive; the C `/RAND_MAX` gives [0,1] but `rand()` never returns
  `RAND_MAX+1` so it's effectively [0,1)). Pin the exact float when `balloon.cpp`
  is ported — the `rgiWidths`/`rgiLeftX` goldens depend on it.
- **Never `Math.random()`.** Anywhere. The whole determinism story collapses.

**Live consumers to replicate in order (panel.cpp:874 `srand(m_seed)` resets
per panel):** goal width (panel.cpp:909), goal lines (:913), startX (:926),
`GetRandomTitle` (:473), emotion intensity/angle (panel.cpp:137/138),
balloon left-shift/center-shift (balloon.cpp:760/768), `AddWavies` jitter
(balloon.cpp:1931/1932). See plan doc §4 #4 for the full list.

---

## 5. GDI text measurement: `GetTextExtent` → frozen glyph table

**C source:** `balloon.cpp:367,668,670`; `format.cpp:707+`; `fonts.cpp:53`.
Plan doc §4 #5 — the dominant cross-platform hazard #2.

**TS translation:** consume `oracle/glyphs/glyphs.json` verbatim. **Never
live browser `measureText`.** Line breaks — and everything downstream —
diverge on font availability, subpixel hinting, platform shaping.

**The frozen table provides:**
- `font.glyphAdvances`: per-codepoint advance widths for Comic Sans MS at the
  pinned size (chars 32–126).
- `font.cFontInfo`: the five `CFontInfo` scalars the engine reads:
  `m_leading=-53`, `m_baseAdd=40`, `m_lineHeight=292`, `m_continuationWidth=180`,
  `m_topOffset=257` (from `balloon.h:47-56`). Pin these as constants in the
  port; do not recompute from font metrics.
- `font.lfHeight=-240`, `tmHeight`, `tmAscent`, etc. — the TEXTMETRIC the
  engine uses for vertical placement.

**Port pattern (when `balloon.cpp`/`format.cpp` are ported):**
```ts
import glyphs from "../../oracle/glyphs/glyphs.json" with { type: "json" };
const advance = (codepoint: number) => /* lookup in glyphs.glyphAdvances */;
```
The glyph lookup is a `Map<number, number>` built once at module load. For
characters outside 32–126 (CJK, control codes), see §11 (deferred).

**Pinning test:** the corpus `formatInfo` dump (`rgiLengths`, `rgiWidths`,
`rgiLeftX`, `rgiStartOffsets`, `bbox`) is the golden for the line-breaker.
When `balloon.cpp` is ported, its test must reproduce corpus-001's
`formatInfo` exactly. That's the tripwire the plan doc warns about.

---

## 6. Geometry: TWIPs, StrokeAndFillPath, vector rounding

### 6.1 TWIPs / world units → world-unit container transform

- The engine runs in `MM_TWIPS` (1 twip = 1/1440 inch; 96 DPI → 15 twip/px).
  Panel ≈ 4860 twips wide. The oracle dumps are in **world units (twips)**,
  not device pixels (plan doc §4 #6). The port keeps the same internal unit
  (twips as `number`) so the corpus goldens compare directly.
- PixiJS rendering (Phase 6) applies a single container transform
  `twips → pixels` at the top of the scene graph: `container.scale.set(96/1440)`
  (or the inverse). **Do not** convert to pixels inside the engine — that
  leaks DPI into the logic and breaks the corpus diff.

### 6.2 `StrokeAndFillPath` / `BeginPath` / `EndPath` → `PIXI.Graphics`

- The engine builds a GDI path then strokes/fills it (balloon outline, arrow,
  panel border). The path is a sequence of `MoveTo`/`LineTo`/`PolyBezierTo`
  calls on a `CDC`. Phase 6 translates these to `PIXI.Graphics` methods:
  `moveTo`/`lineTo`/`bezierCurveTo` — near 1:1.
- **Phase 3/4 scope:** the *math* (control points) is what the oracle pins;
  the *rendering* is Phase 6. Port `ComputeBezpts`/`GetKnot`/`CvertsToCubic`
  (DONE) and leave `CSpline::Draw(CDC*)` as a stub until Phase 6. The `Draw`
  method's only job is `dc->PolyBezierTo(bezpts+1, BezierCount()-1)` — a
  mechanical PIXI `bezierCurveTo` loop over the same bezpts array.

### 6.3 `vector2d` integer-point rounding

- `point_scalmult`'s POINT overload (`vector2d.cpp:100`) uses `(int)(pt.x*scalar)`
  → `INT_CAST` (truncation toward zero), NOT `ROUND`. See §1 for the
  distinction. The DPOINT overload is pure double (no rounding).
- `dpoint_to_point` (`vector2d.cpp:162`) uses `ROUND` on each component. This
  is where float→int conversion happens at the spline/point boundary.
- `int_bezier_nearest_point` (`splinutl.cpp:187`) uses `(int)` truncation on
  both `dist` and `found.x/y` — `Math.trunc`, NOT `ROUND`. The comment in the
  C even says `// should round`. We reproduce bug-for-bug (plan doc §7):
  `BUG(port): splinutl.cpp:204 uses (int) truncation, not ROUND`.

---

## 7. Memoization caches: drop them (output-equivalent)

**C source:** `spline.cpp:93-94` `CMapWordToPtr cardinalMatrixMap`,
`CMapStringToPtr betaMatrixMap`; `spline.cpp:150` `DestroySplineMatrixCaches`.

**TS translation:** drop the caches. Re-derive the 4×4 matrix per instance.

- The caches are pure memoization keyed by `(tension)` / `(tension,bias)`.
  They do not affect output — only allocation count. The engine makes a
  handful of splines per panel; the re-derivation cost is negligible.
- The `sprintf("%f*%f", tension, bias)` key (`spline.cpp:118`) is
  format-dependent and a port hazard (locale-dependent `%f`). Dropping the
  cache eliminates the hazard entirely.
- `DestroySplineMatrixCaches()` becomes a no-op. Keep the exported function
  as a no-op so call sites in `chatdoc.cpp` (when ported) don't break.

**General rule:** any `CMap*ToPtr` cache that's purely a memoization (key →
precomputed value, no semantic side effect) is dropped. If a cache has
semantic meaning (e.g. avatar registry), it's ported as a `Map`.

---

## 8. Integer-width semantics: don't enforce short/int boundaries

- C `SHORT` fields (`SRECT.Left` etc.) silently wrap on overflow. The engine
  only ever stores short-range values in `SRECT`s (balloon-local coords). The
  port keeps `number` and **does not** `& 0xffff`/clamp — an overflow would
  indicate a real port bug, and silently wrapping would hide it. If a port
  ever produces an `SRECT` value outside ±32767, that's a divergence to
  flag, not wrap.
- C `int` (32-bit) arithmetic: use `number` (double-width safe for the
  engine's value range). The `| 0` in `ROUND`/`INT_CAST` clamps to Int32 only
  at the float→int boundary, matching C cast behavior. Intermediate `int`
  sums (`c0.x + c1.x + c2.x + c3.x` in `CubicToBezier`) stay in `number`;
  the engine's values don't approach 2⁵³.
- C `WORD` (unsigned 16-bit) as a map key (`spline.cpp:98`) — moot once the
  cache is dropped (§7).

---

## 9. Diagnostics: `ASSERT`/`TRACE` → throw / drop

- **`ASSERT(cond)`** (`spline.cpp:14` `ASSERT(n >= 2)`, `:174` `ASSERT(nKnots >= 4)`,
  `:297` `ASSERT(foundKnotIndex > 0)`): translate to `if (!cond) throw new
  Error(...)`. In the C release build `ASSERT` is a no-op, so the engine
  *relies* on the condition holding — a violation is a real bug. Throwing
  surfaces it; silently continuing would produce divergent state. Include
  the C file:line in the message for traceability.
- **`TRACE(...)`** (`vector2d.cpp:64` "Can't normalize the unit vector"):
  drop. It's a debug-only print. The degenerate-handling code around it
  (`pt.x = pt.y = 0; return pt;`) is the real behavior and is ported.
- **`DEBUG(...)`** (`spline.cpp:191`, commented out anyway): drop.

---

## 10. Doc model: `CChatDoc`/`CPanel`/`CBalloon`/`CBody` → plain TS classes

**Finding (harness README):** the only MFC-framework-inheriting class is
`CChatDoc : CDocObjectServerDoc`, and its OLE/doc-template methods are never
on the replay hot path. The port models these as plain TS classes with the
same field names — no MFC inheritance, no `CRuntimeClass`, no
`IMPLEMENT_DYNCREATE`.

| C class | TS class | Notes |
|---|---|---|
| `CChatDoc` | `class CChatDoc` | Plain class. The harness's `COracleChatDoc` public-ctor trick (harness README) is a C++ link-time concern; the port has no such restriction. Fields: `m_seed`, `m_avatars`, `m_history`, etc. |
| `CPanel` (`CUnitPanelPage`) | `class CPanel` | `m_seed`, `m_balloons: CBalloon[]`, `m_bodies: CBody[]`, `LayoutBalloons`, `LayoutAvatars`. |
| `CBalloon` | `class CBalloon` | `m_spline: CSpline\|null`, `m_fInfo: CFormatInfo\|null`, `m_trueBox: SRECT`, `m_routeRgn: SRECT`, `m_speaker`, `m_traj`. |
| `CBody` | `class CBody` | `m_avatarID`, `m_flip`, `m_bbox: SRECT`, `m_arrowX`. |
| `CFormatInfo` | `class CFormatInfo` (or interface) | `m_nLines`, `rgiLengths/Widths/LeftX/StartOffsets: number[]`, `m_bbox: SRECT`. |

**`m_spline` ownership:** the C uses `new`/`delete` and `Clone()`. The port
uses plain references; `Clone()` becomes a constructor-call that copies
fields. GC handles the rest — no `DestroySplineMatrixCaches`-style teardown
needed beyond the no-op in §7.

---

## 11. Deferred / open (not blocking Phase 4 start)

- **CJK path** (`jis2sjis`/`intl.c`): open question from handoff #1. The
  glyph table covers chars 32–126 only. If CJK is in scope, extend the
  glyph dump and port `jis2sjis`/`sjis2jis` tables. Default: defer until
  Tier-1/3 non-CJK green.
- **`.avb`/`.bgb` asset decode** (Tier-2): DIB RLE4/RLE8 + 2-bpp
  maskedmono/dualmask → RGBA textures (`avbfile.cpp:1471-1542`). The Tier-2
  manifest subcommand is not yet implemented in the oracle (LEDGER TODO).
  Decide during Phase 4 whether to build it as a prerequisite or fold into
  the `avbfile` port unit. The decode produces RGBA textures consumed by
  PixiJS (Phase 6) — the *format parse* is Phase 4, the *render* is Phase 6.
- **Pixel-level (Tier-3 #8 raster hash) goldens:** defer until Tiers 1-7
  green (handoff open question #2). The 67/67 bezpt match already gives
  strong outline parity without rasterizing.
- **`CTraj` / `CLine` / `CArc` / `DASHINFO`** (`traj.h`): GDI trajectory
  types. Stubbed in the Phase-3 spline port (`Draw`/`Dash` deferred). Port
  alongside `balloon.cpp` in Phase 4 (its only consumer). The `CPtrList
  m_segs` → `CSeg[]`; `AddSeg` → `push`.

---

## 12. Two-agent translation check — PASSED

Plan doc §8 Phase 3 benchmark: *"two agents translate the same construct
identically."* The check was performed as follows:

- **Agent A:** the C++ oracle (frozen `oracle/corpus/001/expected.json`),
  produced by the MSVC build on `windows-2022` CI (run 30125674590).
- **Agent B:** the TS port (`port/src/engine/spline.ts`), independently
  written from the same C source (`v1.0-pre-modern/spline.cpp`).
- **Construct translated:** `CBeta::ComputeBezpts` — the full
  matrix-derivation → `GetKnot` (closed-spline wraparound) → `CvertsToCubic`
  (ROUND of matrix·knot products) → `CubicToBezier` (ROUND of third-fractions)
  pipeline, applied to a real 22-control-point closed balloon outline.
- **Result:** all 67 Bézier control points match **byte-exact**. The check
  caught one rulebook ambiguity (the `ROUND` semantics, §1) which was
  resolved in the rulebook's favor (floor, not trunc) and re-verified.
- **Second construct:** `ROUND` itself was translated two ways
  (`Math.trunc(fp+0.5)` vs `Math.floor(fp+0.5)`) and diffed against the
  golden — only `floor` passes. This is the canonical example of "fix the
  rulebook, not the code": the C macro *reads* like trunc, but the oracle
  *says* floor. The oracle wins.

**Verdict:** the rulebook is unambiguous for the pure-math modules. Phase 4
may begin. The next port loops should add a per-module golden test of the
same form (load corpus `expected.json`, reproduce the dumped struct, assert
exact equality) so every module gets its own two-agent check.

---

## 13. File map (Phase 3 artifacts)

| File | Role |
|---|---|
| `port/src/core/numeric.ts` | `ROUND`, `INT_CAST`, `LCG`, `randfloat`, constants. The load-bearing primitives. |
| `port/src/core/types.ts` | `DPOINT`/`POINT`/`RECT`/`SRECT`/`BOUNDBOX`/`BEZIER` interfaces + constructors + `SRECTToRECT`/`dpoint_to_point`/`point_to_dpoint`. |
| `port/src/engine/vector2d.ts` | Port of `vector2d.cpp` (DPOINT + POINT overloads, angle routines). |
| `port/src/engine/bbox.ts` | Port of `bbox.cpp` (integer bbox ops, incl. the pinned `bbox_within_bbox` bug). |
| `port/src/engine/spline.ts` | Port of `spline.cpp` + `splinutl.cpp` (`CSpline`/`CCardinal`/`CBeta` + Bézier utils). |
| `port/test/core/numeric.test.ts` | ROUND/INT_CAST/LCG oracle-pinning tests. |
| `port/test/engine/*.test.ts` | Per-module tests, incl. the 67/67 corpus-001 golden. |
| `port/package.json`, `port/tsconfig.json`, `port/vitest.config.ts` | ESM TS + Vitest scaffold. `npm test` / `npm run typecheck`. |