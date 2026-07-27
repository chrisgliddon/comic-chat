# Comic Chat Port Rulebook: MFC/GDI → TypeScript/PixiJS Idiom Map

*Phase 3 deliverable of `docs/porting/TEST-ORACLE-PLAN.md` §8. The contract
that lets Phase 4 port loops run mechanically without per-file re-litigation.
Every entry here is either (a) pinned by an oracle golden, (b) backed by a
shakedown test in `port/test/`, or (c) a direct consequence of the plan doc's
determinism preconditions (§4). When two agents translate the same C
construct and differ, THIS document is the referee — fix the rulebook, not
the code.*

*Status: Phase 3 COMPLETE + Phase 4 in progress (avatario + avatar
ported and oracle-golden-verified; 180/180 tests pass; 3 frozen golden
dumps: textpose, avatario, avatar). The 3-file shakedown port
(`vector2d`, `bbox`, `spline`/`splinutl`) is green: 67/67 exact-match
of the CBeta Bézier control points against `oracle/corpus/001/expected.json`.
The single most important finding — the `ROUND` semantics — is §1 below.*

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

### 1.1 Float-cast (`(float)`) → `Math.fround`

The C engine casts double→float32 in several `#define`s (notably the `EM_*`
emotion constants, `avatar.h:254-261`: `((float)(k * 2 * PI / 8))`). JS
numbers are doubles; to match the float32 bit pattern the oracle emits, use
`Math.fround(x)`. This was caught by the textpose oracle golden: the
emotion values (4.7123889923095703 for SHOUT, 3.1415927410125732 for SAD,
etc.) only match when (a) PI is the v2.5 full-precision value (see §1.2)
AND (b) the result is fround'd. Without fround, the TS produces
4.712384999999999 (the double) which is a different bit pattern.

### 1.2 PI is tree-dependent

**v1.0-pre-modern** `vector2d.h:39`: `#define PI 3.14159` (low precision).
**v2.5-beta-1-modern** `vector2d.h:54`: `#define PI 3.14159265358979323846`
(full precision).

The oracle goldens come from v2.5. The emotion constants use the v2.5 PI
(and fround). The spline/bbox/vector2d pure-math ports used the v1.0-pre PI
(3.14159) for the angle routines (`degrees_to_rads`, `value_to_angle`, etc.)
and got 67/67 bezpt match — because the spline code never touches PI. The
two PI values coexist in the port: `port/src/core/numeric.ts` exports the
v1.0-pre `PI = 3.14159` for the vector2d angle routines; `port/src/core/
emotion.ts` uses its own full-precision `PI_V25` for the emotion constants.
When porting a module that uses `PI`, check which tree's `vector2d.h` it's
compiled against (the oracle tree is v2.5) and use the matching constant.

**The avatario/avatar port uses the v2.5 PI for the angle metric** (see
§14 below). The `subtract_angles` / `value_to_angle` calls in
`port/src/engine/avatar.ts` use a local `PI_V25 = 3.14159265358979323846`
because the threshold `thisAngle < PI/8` is sensitive to the PI precision
(the threshold is `~0.39269908` with v2.5 vs `0.3927` with v1.0-pre, and
the difference flips the selection for some sentinel-emotion inputs).
The avatario/avatar dump is pinned to the v2.5 forms; the spline golden
remains pinned to the v1.0-pre forms. The two coexist: `port/src/core/
numeric.ts` (spline/vector2d consumers) uses v1.0-pre, `port/src/engine/
avatar.ts` (avatario/avatar consumers) uses v2.5.

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
- **`textpose.cpp:300-313` sentence-start loop** — `BUG(port): lptr is dead
  code; StartCompare2 uses buff/lower (whole-string start) not bptr/lptr`.
  The loop iterates sentences via `bptr = GetNextSentenceStart(bptr)` and
  computes `lptr = lower + (bptr - buff)`, but the `StartCompare2` calls pass
  `buff`/`lower` (the whole string), not `bptr`/`lptr`. So only the first
  sentence's beginning is ever effectively checked (re-tested per iteration).
  "Well. Hello there" does NOT fire WAVE because "Well" ≠ "Hello" at the
  whole-string start. The oracle textpose golden pins this.

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
| `port/src/engine/vector2d.ts` | Port of `vector2d.cpp` (DPOINT + POINT overloads, angle routines). Uses v1.0-pre PI (pinned by spline golden). |
| `port/src/engine/bbox.ts` | Port of `bbox.cpp` (integer bbox ops, incl. the pinned `bbox_within_bbox` bug). |
| `port/src/engine/spline.ts` | Port of `spline.cpp` + `splinutl.cpp` (`CSpline`/`CCardinal`/`CBeta` + Bézier utils). |
| `port/src/core/emotion.ts` | `EM_*` emotion constants (v2.5 PI + fround) + `MAXEMOPTS`/`OVERRIDEBYPRIORITY`/`ADDPRIORITY`. |
| `port/src/core/emotionopts.ts` | `CEmotion`/`CEmotionOpts` (the emotion accumulator). |
| `port/src/engine/textpose.ts` | Port of `textpose.cpp` (text → emotion rules). |
| `port/src/engine/avatario.ts` | Port of `avatario.cpp` (emotion quantization round-trip). |
| `port/src/engine/avatar.ts` | Port of `avatar.cpp`'s `GetBodyFromEmotion` family (emotion → pose). Uses v2.5 PI. |
| `port/test/core/numeric.test.ts` | ROUND/INT_CAST/LCG oracle-pinning tests. |
| `port/test/engine/*.test.ts` | Per-module tests, incl. oracle-golden differential tests for textpose/avatario/avatar. |
| `port/package.json`, `port/tsconfig.json`, `port/vitest.config.ts` | ESM TS + Vitest scaffold. `pnpm test` / `pnpm run typecheck`. |

---

## 14. Avatario / Avatar port idioms (Phase 4)

### 14.1 `emFloats[]` and the avatario wire encode

**C source:** `avatario.cpp:45-98`. The 17-entry emotion-wheel table; the
wire encode/decode for the UDI protocol and the `.ccc` transcript.

- The 8 directional emotions (`EM_HAPPY`..`EM_LAUGH`) sit on the wheel at
  `k * 2*PI/8`; their values are **float32 bit patterns** (C `(float)`
  cast). Use `Math.fround` in the port. See §1.1.
- The 8 special emotions (`EM_WAVE`..`EM_3QFWALK`) are integer-valued
  sentinels (1001..1008). No float-cast precision issue.
- `EM_NEUTRAL` is `0.0` (NOT the same as `EM_HAPPY` even though both are
  zero in float — `emFloats[9]` is explicitly `EM_NEUTRAL` to distinguish
  the wheel-zero slot from the neutral-sentinel slot). Both `0.0` and
  `EM_NEUTRAL` encode to `emVal=1` (the linear search in `EmotionToBytes`
  starts at `i=1` and stops on the FIRST match; `emFloats[1]=EM_HAPPY=0.0`
  matches before `emFloats[9]=EM_NEUTRAL=0.0`).
- `IndexToByte` / `ByteToIndex` are pure ASCII-digit conversion
  (`byteIn + '0'` / `byteIn - '0'`). The protocol byte range is 0x30..0x40
  (emVal 0..16).
- `EmotionToBytes` intensity: `(BYTE)(em.m_intensity * 10)` is **truncation
  toward zero** (C `(BYTE)x`), not `ROUND`. Use `INT_CAST(em.m_intensity *
  10) & 0xff` (§1 distinguishes ROUND vs INT_CAST).
- `BytesToEmotion` out-of-range `emIndex` → `EM_NEUTRAL`. Out-of-range
  is `< 0` or `>= emFloats.length` (currently 18).
- `EmotionToFloat` out-of-range → `0.0` (NOT `emFloats[0]` — the C returns
  the literal `0.0`, even though `emFloats[0]` happens to be `0.0` today).

### 14.2 PI precision for the angle metric (avatar GetBodyFromEmotion)

**C source:** `avatar.cpp:226-416`. The `subtract_angles` / `value_to_angle`
functions use the oracle tree's `PI` (v2.5 = `3.14159265358979323846`).

The port's `port/src/engine/vector2d.ts` exports a `subtract_angles` that
uses the v1.0-pre `PI = 3.14159` (pinned by the Phase-3 spline golden —
spline never touches PI). For most consumers this is fine (the
normalization path is the same shape), but the **threshold check
`thisAngle < PI/8`** in `GetBodyFromEmotion`'s torso/body scan is
sensitive to the PI precision:

- v2.5 PI: `PI/8 ≈ 0.39269908169872414`
- v1.0-pre PI: `PI/8 = 0.39269875`

The difference is small but real, and for some sentinel-emotion inputs
(e.g. `EM_WAVE=1001` vs an `EM_SCARED` bRec entry) the normalized angle
comes out to a value between the two thresholds, flipping the selection.

**Rule:** the avatario/avatar port uses the v2.5 PI for the angle
metric. `port/src/engine/avatar.ts` declares `PI_V25 =
3.14159265358979323846` locally and provides `value_to_angle_v25` /
`subtract_angles_v25`. The `port/src/engine/vector2d.ts` exports
remain at v1.0-pre (spline golden). When porting a new module that
uses the angle metric AND is sensitive to the threshold, decide
which PI to use based on which oracle dump it's compared against.

### 14.3 `m_lastBody` / `m_lastFace` / `m_lastTorso` history state

**C source:** `avatar.cpp:755-770` (`RecordBody`). The m_last* fields are
**NOT** updated by `GetBodyFromEmotion` itself — they're updated by
`RecordBody` in the consumer's `UpdateBody` flow. The selection returns
a new `CBody*`; the caller applies it via `av->UpdateBody(body)` which
calls `av->RecordBody(body)` which writes the chosen index into m_last*.

In the dump, the harness constructs a fresh avatar per probe and never
calls `UpdateBody`, so m_last* stays at the ctor default (`-1`).

**Bug caught in the port (fixed):** the initial port wrote
`this.m_lastBody = body.m_bodyIndex` at the end of each
`GetBodyFromEmotion` call. The C doesn't do this in the selection
path — only the consumer does. The TS dump then disagreed with the
C dump on `m_last*`. Fix: remove the writes from all four
`GetBodyFromEmotion` variants (Simple + Complex, single-emotion +
CEmotionOpts). The TS dump and the unit tests now assert m_last*
stays `-1` after selection.

### 14.4 C quirks (reproduce bug-for-bug)

The `GetBodyFromEmotion` family has several C quirks that the port
must reproduce:

- **Angle-after-normalization coincidence** (avatar.cpp:230-249).
  The C's torso/body scan checks `bRec[index].emotion > 7` to skip
  sentinel entries in the *bRec*, but it does NOT check the input
  emotion. When the input is a sentinel (e.g. `EM_WAVE=1001`), the
  loop still computes the normalized angle to each directional
  bRec entry. The angle from `EM_SCARED` (2.356) to 1001 normalizes
  to `~0.3827`, which is `< PI/8` — so SCARED wins. The C source
  acknowledges this with "Distance metric needs rethinking!".
  The port reproduces this; the dump pins it.
- **GetHeadAndBodyFromEmotion sets only ONE index per call**
  (avatar.cpp:302-328). The directional branch sets the face index
  but leaves the torso index at `-1`; the sentinel branch sets the
  torso index but leaves the face at `-1`. The `CEmotionOpts` path
  iterates by descending priority and relies on this — a single
  opt call may fill face OR torso but not both. The CEmotionOpts
  variant then calls `SetFaceNeutral` / `SetTorsoNeutral` for any
  unfilled index. So a directional emotion opt always leaves the
  torso at NEUTRAL fallback (the directional branch didn't set
  tIndex).
- **The `> 7` filter is on bRec entries, not on the input emotion**.
  The C `if (bRec[index].emotion > 7) continue;` only skips sentinel
  bRec rows; the input can be anything and the directional entries
  are still considered.
- **`emVal` linear search starts at `i=1`** in `EmotionToBytes` —
  `emFloats[0]` is `0.0` (padding) and is never reached by the
  encoder. The first match for `0.0` is `emFloats[1]=EM_HAPPY=0.0`,
  so `emVal=1` (NOT `emVal=9` for `EM_NEUTRAL`).

These are all pinned in the dump; deviating from any of them
will surface as a golden mismatch in `port/test/engine/
avatar_oracle.test.ts` and `port/test/engine/avatario_oracle.test.ts`.
## 15. `.avb` / DIB asset-format port idioms (Tier-2)

Pinned by `--avb`, frozen as `oracle/avb/<stem>.golden.json` (one manifest per
ComicArt asset). The manifest has a pre-load half (per pose, the three
`(offset, format, paletteType)` triples) and a post-load half (per image slot,
the `BITMAPINFOHEADER` scalars plus a CRC32 of the decoded pixels), so a
divergence tells you *which* layer broke.

### 15.1 Zero-initialise the record tables — do NOT mirror the constructors

`CAvatarX::Initialize` (avatar.cpp:886) covers only base-class members.
`CAvatarComplex()` sets just `m_lastFace`/`m_lastTorso`, `CAvatarSimple()` just
`m_lastBody`. So `fRec`, `bRec`, `nFaces`, `nTorsos` and `m_nBodies` are
**indeterminate** until `LoadFaceRecs` / `LoadTorsoRecs` / `LoadBodyRecs` writes
the pointer and the count as a pair (avbfile.cpp:1077, 1181, 1255).

The engine gets away with it because every shipped asset carries the
`AK_NFACES`/`AK_NTORSOS`/`AK_NBODIES` record. A file missing one leaves *both*
the pointer and the count garbage — so a null check on the pointer is not enough,
and iterating `count` entries is an out-of-bounds read.

**Port rule:** initialise these to `null` / `0` and treat a missing record as
"no table", not as "trust the count". This is one of the few places the port must
be *safer* than the original rather than bug-for-bug: the C behaviour here is
undefined, not a quirk with an observable to reproduce.

### 15.2 RLE is expanded during load; the manifest CRC is over expanded pixels

`CAvatarDIB::Load` ends with an unconditional `ConvertToNonRLE()`
(avbfile.cpp:462 — the comment blames "a bug in windows that mandates drawing be
confined to non-rle bitmaps"). So **no caller ever sees an RLE DIB**: by the time
a `CPose` hands out `GetDrawing()`/`GetMask()`/`GetAura()`, `biCompression` is
`BI_RGB` and the bits are expanded.

**Port rule:** expand RLE4/RLE8 at load time, not lazily at draw time. The
Tier-2 `pixelCrc32` is taken over *expanded* pixels, so a port that keeps the
compressed bytes and hashes those diverges on every RLE asset even though its
decoder is correct.

Then, on the length:

- `biSizeImage` is documented as "may be 0" for uncompressed DIBs, and `dib.cpp`
  sets it to 0 on the paths that build headers by hand (dib.cpp:295, 377). So for
  a DIB that was already `BI_RGB` in the file, it may be 0 and must not be used
  as the length — that is `StorageWidth() * abs(biHeight)`.
- The RLE expanders *do* set it: `biSizeImage = newSize` alongside
  `biCompression = BI_RGB` (dib.cpp:930-931, 1011-1012). They also *read* it as
  the input length, walking to `m_pBits + biSizeImage` (dib.cpp:892, 957).

Both the computed length and `biSizeImage` are in the manifest, so picking the
wrong one surfaces as a length mismatch rather than a silent CRC drift. A
non-zero `biSizeImage` equal to `storageWidth * abs(biHeight)` is also the only
hint the manifest currently carries that an asset *was* RLE on disk — see the
observability gap noted in oracle/LEDGER.md.

Note `biHeight` may be negative (top-down DIB) — take the absolute value for the
byte extent, and keep the sign as row order.

### 15.3 `DIBStorageWidth` rounds before it divides

    if (nBitCount < 8) nWidth += (8 / nBitCount) - 1;
    return (((nWidth * nBitCount) / 8) + 3) & ~3;

(dib.cpp:1027.) The pre-round makes the integer division behave as a ceiling, so
this is arithmetically `align4(ceil(width * bpp / 8))` — but transliterate the
original rather than the simplification, and check the result against the
manifest's `storageWidth` instead of trusting either derivation.

### 15.4 `COLORREF` is `0x00BBGGRR` — byte 0 is RED

`RGB(r,g,b)` is `r | (g << 8) | (b << 16)`, so the **low** byte of a `COLORREF`
is red, while an `RGBQUAD` is laid out B, G, R, reserved — the reverse. A port
that treats `COLORREF` as `0xRRGGBB`, or that copies `RGBQUAD` bytes straight
into one, gets channel-swapped avatars. Manifest palettes are emitted as hex so a
swap is visible on sight.

The two conversion macros (avbfile.h:8-11) look inconsistent but are a correct
round-trip, and only one of them is safe to transliterate:

    GET_COLORREF_FROM_RGBQUAD(prgb)  RGB((prgb)->rgbRed, ->rgbGreen, ->rgbBlue)
    SET_RGBQUAD_FROM_COLORREF(prgb, c)  *(COLORREF*)(prgb) =
                                          RGB(GetBValue(c), GetGValue(c), GetRValue(c))

GET reads named fields — port it as written. SET looks like it swaps the channels,
but it does not: it builds `B | (G << 8) | (R << 16)` and **type-puns it as a
DWORD store into the `RGBQUAD`**, which on little-endian lands B in `rgbBlue`, G
in `rgbGreen`, R in `rgbRed`, 0 in `rgbReserved` — the exact inverse of GET. The
inverted argument order is compensating for the punned store, not reordering
channels.

**Port rule:** assign the named fields (`rgbBlue = b` …). Transliterating SET's
argument order into a field-wise assignment channel-swaps the palette, and the
DWORD-store trick has no meaning in TS anyway. Note it also zeroes
`rgbReserved` as a side effect, which a field-wise version must do explicitly.

### 15.5 Three format branches have no golden — by corpus, not by omission

Every shipped asset is magic `0x8181` (`AF_MAGICNUM_NEW`) version 2. So the
following have **no possible Tier-2 golden from this tree**:

- the old `0x81` (`AF_MAGICNUM`) container,
- the old record tags (`AK_NAME`=1 … `AK_NBODIES2`=12 under 256) and the
  `olddata` struct layouts with their `byPadding[16]`,
- the plain-DIB `'BM'` backdrop branch (avbfile.cpp:1669), and `.bmp`
  backdrops generally.

Port them if you like, but do not read a green Tier-2 as covering them. See
oracle/LEDGER.md "Measured: what the shipped corpus does NOT cover".

### 15.6 Backdrop world metadata is hardcoded, not in the file

`SetBackDropAux` (backdrop.cpp:91-95) stamps every backdrop with
`xdim = ydim = 315`, world `(0, 0, 4860, -4860)`, `normHeight = 100`. Nothing in
the `.bgb` carries it. Port the constants; the `.bgb` manifest deliberately does
not include them because they are not a property of the asset.
