/**
 * Numeric primitives that reproduce MSVC/CRT behavior byte-for-byte.
 *
 * Source of truth: `v1.0-pre-modern/vector2d.h` macros and the MSVC CRT `rand()`.
 * These helpers are the load-bearing core of the rulebook (RULEBOOK.md §ROUND,
 * §RNG) — every downstream module MUST import from here so a single audit point
 * governs float-to-int and PRNG semantics.
 */

// ---------------------------------------------------------------------------
// ROUND  —  the dominant cross-platform hazard
// ---------------------------------------------------------------------------
// C macro (vector2d.h:37):  #define ROUND(fp) ((int)(fp + 0.5))
//
// ROUND — the dominant cross-platform hazard.
// ---------------------------------------------------------------------------
// C macro (vector2d.h:37):  #define ROUND(fp) ((int)(fp + 0.5))
//
// EMPIRICAL BEHAVIOR (pinned by oracle corpus-001, NOT by the C standard):
// the MSVC `ROUND` macro behaves as `floor(fp + 0.5)` — round-half-up — for
// the values the engine actually produces. This was discovered by the Phase-3
// two-agent check: the C++ oracle's CBeta bezpts (67 control points) match the
// TS port EXACTLY only when ROUND uses `Math.floor(fp + 0.5)`, and match 0/67
// when ROUND uses `Math.trunc(fp + 0.5)`.
//
// Why the divergence from the C standard? `(int)(-61.147)` truncates toward
// zero = -61 in standard C. But the golden says -62 (= floor(-61.147)). The
// most likely explanation is MSVC's `/arch:SSE2` (or compiler optimization of
// the macro) emitting a native rounding instruction that rounds half toward
// -Infinity for negative inputs, OR the `(int)` cast on x87 producing
// different rounding. The mechanism doesn't matter — the ORACLE is the
// referee, and the oracle says floor(fp+0.5). This is THE rulebook's most
// load-bearing decision.
//
// Comparison with JS builtins:
//   - ROUND(-61.647) = floor(-61.147) = -62   <-- the oracle-pinned value
//   - Math.round(-61.647) = -62               (agrees, half away from zero)
//   - Math.trunc(-61.647 + 0.5) = -61         (WRONG — what naive porting gives)
//   - Math.round(-2.5) = -2  (half toward +Inf); ROUND(-2.5) = floor(-2.0) = -2 (agrees)
//   - Math.round(-2.6) = -3; ROUND(-2.6) = floor(-2.1) = -3 (agrees)
//   - Math.round(-1.1) = -1; ROUND(-1.1) = floor(-0.6) = -1 (agrees)
//   - Math.round(2.5) = 3;  ROUND(2.5) = floor(3.0) = 3 (agrees)
// In fact floor(fp+0.5) === Math.round(fp) for ALL fp except negative exact
// .5 values where Math.round ties toward +Inf and floor(fp+0.5) ties toward
// -Inf. Pin BOTH via the oracle golden; do not "simplify" to Math.round.
//
// Negative-zero: Math.floor(-0.5 + 0.5) = Math.floor(0) = 0 (no -0). Good.
export function ROUND(fp: number): number {
  return Math.floor(fp + 0.5) | 0;
}

// MSVC `(int)x` cast — used for the integer POINT overloads (vector2d.cpp:100
// `point_scalmult`). C `(int)x` truncates toward zero; we match that here (NOT
// the floor behavior of ROUND — these are distinct operations in the engine).
// The `| 0` clamps into Int32 (matching C 32-bit int overflow) and normalizes
// -0 to +0 (C int has no negative zero).
export function INT_CAST(fp: number): number {
  return (Math.trunc(fp)) | 0;
}

// ---------------------------------------------------------------------------
// Constants — pinned verbatim from vector2d.h
// ---------------------------------------------------------------------------
export const LARGENUMBER = 1e24;
export const SMALLNUMBER = 1e-24;
export const LARGEINTEGER = 100000000;
export const LARGESHORT = 31000;
// vector2d.h:39 uses the low-precision literal 3.14159 — keep it, do NOT
// substitute Math.PI (that would shift every angle result).
export const PI = 3.14159;
export const TWO_PI = 2 * PI;

export const MAX = (a: number, b: number): number => (a > b ? a : b);
export const MIN = (a: number, b: number): number => (a < b ? a : b);
export const ABS = (a: number): number => (a >= 0 ? a : -a);

// ---------------------------------------------------------------------------
// LCG  —  MSVC CRT rand()/srand() replication
// ---------------------------------------------------------------------------
// Plan doc §4 #4: `seed = seed*214013 + 2531011; return (seed>>16) & 0x7fff`,
// RAND_MAX = 0x7FFF. Call order is load-bearing (see balloon.cpp:760/768
// zero-effect ShiftLines that still advance the sequence). The port must
// consume this exact stream; never Math.random.
//
// Seed state is 32-bit unsigned; emulate with `>>> 0` after the multiply/add
// to stay in Uint32 range (JS `*` is double, but the bit pattern is exact for
// 32-bit operands and `>>> 0` reifies it).
export const RAND_MAX = 0x7fff;

export class LCG {
  private state: number;

  constructor(seed: number) {
    // srand stores the seed as-is (CRT) — do not pre-mix.
    this.state = seed >>> 0;
  }

  rand(): number {
    // MSVC: seed = seed * 214013 + 2531011 (mod 2^32); return (seed>>16) & 0x7fff
    this.state = (Math.imul(this.state, 214013) + 2531011) >>> 0;
    return (this.state >>> 16) & 0x7fff;
  }

  srand(seed: number): void {
    this.state = seed >>> 0;
  }

  // Internal: expose current state for determinism ledgers (never consumed
  // by the engine itself).
  get _state(): number {
    return this.state >>> 0;
  }
}

// ---------------------------------------------------------------------------
// randfloat  —  balloon.cpp:428 consumer
// ---------------------------------------------------------------------------
// Replicates the engine's `randfloat()` which scales rand() into [0,1).
// Kept here so RNG call order is centralized; callers must go through LCG.
// (Exact float arithmetic of the original is pinned in the rulebook; the
// balloon port will assert against the oracle's rgiWidths dump.)
export function randfloat(rng: LCG): number {
  return rng.rand() / (RAND_MAX + 1);
}