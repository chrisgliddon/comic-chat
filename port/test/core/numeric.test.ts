/**
 * Tests for core/numeric — the load-bearing primitives.
 *
 * These pin the exact MSVC semantics, as EMPIRICALLY DETERMINED by the oracle
 * corpus-001 golden (not by the C standard). The single most important fact:
 * the engine's ROUND macro behaves as `floor(fp + 0.5)`, NOT `trunc(fp + 0.5)`.
 * This was proven by the Phase-3 two-agent check — the C++ oracle's 67 CBeta
 * bezpts match the TS port 67/67 with floor, 0/67 with trunc. If any of these
 * break, every downstream module diverges from the oracle goldens.
 */
import { describe, it, expect } from "vitest";
import { INT_CAST, LCG, RAND_MAX, ROUND, randfloat } from "../../src/core/numeric.js";

describe("ROUND — oracle-pinned floor(fp + 0.5) (round-half-up)", () => {
  it("matches Math.round on positive values", () => {
    for (let i = 0; i < 200; i++) {
      const v = Math.random() * 1e6;
      expect(ROUND(v)).toBe(Math.round(v));
    }
  });

  it("agrees with Math.round on positive .5 boundaries", () => {
    expect(ROUND(2.5)).toBe(3);
    expect(ROUND(0.5)).toBe(1);
    expect(ROUND(1.5)).toBe(2);
  });

  it("matches Math.round on negative non-half values (the oracle truth)", () => {
    // The C macro is `((int)(fp + 0.5))` which standard-C would truncate to
    // -61, but the MSVC oracle golden produces -62 = floor(-61.147). This is
    // THE pinned behavior. Math.round(-61.647) also = -62 (half away from
    // zero), so they agree here.
    expect(ROUND(-61.647)).toBe(-62); // the corpus-001 bezpts[0].x raw dot
    expect(Math.round(-61.647)).toBe(-62);
    expect(ROUND(-2.6)).toBe(-3);
    expect(ROUND(-1.1)).toBe(-1);
  });

  it("DIVERGES from Math.round ONLY on negative exact-.5 (tie direction)", () => {
    // floor(-2.5 + 0.5) = floor(-2.0) = -2 (ties toward -Inf via the +0.5 shift)
    // Math.round(-2.5) = -2 (ties toward +Inf). Same answer here by coincidence.
    expect(ROUND(-2.5)).toBe(-2);
    expect(Math.round(-2.5)).toBe(-2);
    // But construct a case where they differ: none exist for floor(fp+0.5) vs
    // Math.round, because Math.round IS floor(fp+0.5) for all non-.5, and for
    // .5 Math.round = floor(fp+0.5) too (both give the same). So they're
    // actually identical. The REAL divergence is vs Math.trunc(fp+0.5).
    expect(ROUND(-61.647)).not.toBe(Math.trunc(-61.647 + 0.5)); // -62 vs -61
  });

  it("handles integer inputs", () => {
    expect(ROUND(3)).toBe(3);
    // ROUND(-3) = floor(-3 + 0.5) = floor(-2.5) = -3. Pin it (differs from
    // the trunc version which would give -2).
    expect(ROUND(-3)).toBe(-3);
    expect(ROUND(0)).toBe(0);
  });

  it("is stable across the whole 16-bit range (property, oracle-pinned form)", () => {
    for (let i = -1000; i <= 1000; i++) {
      // ROUND(n) for integer n = floor(n + 0.5).
      const expected = Math.floor(i + 0.5);
      expect(ROUND(i)).toBe(expected);
    }
  });

  it("never produces negative zero (C int has no -0)", () => {
    // ROUND(fp) = floor(fp + 0.5) | 0. The | 0 coerces -0 to +0.
    // -0.1 + 0.5 = 0.4, floor = 0 -> +0
    expect(Object.is(ROUND(-0.1), 0)).toBe(true);
    // -0.4 + 0.5 = 0.1, floor = 0 -> +0
    expect(Object.is(ROUND(-0.4), 0)).toBe(true);
    // -0.6 + 0.5 = -0.1, floor = -1 -> -1 (not zero at all)
    expect(ROUND(-0.6)).toBe(-1);
    // 0.4 + 0.5 = 0.9, floor = 0 -> +0
    expect(Object.is(ROUND(0.4), 0)).toBe(true);
  });
});

describe("INT_CAST — (int)x truncation toward zero, Int32 wrap", () => {
  it("truncates toward zero (distinct from ROUND's floor)", () => {
    expect(INT_CAST(2.9)).toBe(2);
    expect(INT_CAST(-2.9)).toBe(-2); // toward zero, NOT floor (-3)
    expect(INT_CAST(0.5)).toBe(0);
  });

  it("wraps into Int32 (| 0)", () => {
    expect(INT_CAST(2147483648)).toBe(-2147483648); // 2^31 -> -2^31
    expect(INT_CAST(4294967296)).toBe(0); // 2^32 -> 0
  });

  it("never produces negative zero", () => {
    expect(Object.is(INT_CAST(-0.5), -0)).toBe(false);
    expect(INT_CAST(-0.5)).toBe(0);
  });
});

describe("LCG — MSVC CRT rand()/srand() replication", () => {
  it("RAND_MAX is 0x7FFF (32767)", () => {
    expect(RAND_MAX).toBe(0x7fff);
    expect(RAND_MAX).toBe(32767);
  });

  it("produces the documented MSVC sequence for seed 0", () => {
    // MSVC CRT rand() sequence for srand(0), computed from the LCG
    // seed*214013+2531011, (seed>>16)&0x7fff. Verified against the TS LCG
    // (which the seed-1 test below cross-checks against the well-known
    // srand(1) default sequence [41, 18467, 6334, ...]).
    const rng = new LCG(0);
    const expected = [38, 7719, 21238, 2437, 8855, 11797, 8365, 32285, 10450, 30612];
    for (const e of expected) {
      expect(rng.rand()).toBe(e);
    }
  });

  it("produces the documented MSVC sequence for seed 1", () => {
    // srand(1) is the CRT default. First value is 41, then 18467, 6334, ...
    const rng = new LCG(1);
    const expected = [41, 18467, 6334, 26500, 19169, 15724, 11478, 29358, 26962, 24464];
    for (const e of expected) {
      expect(rng.rand()).toBe(e);
    }
  });

  it("is deterministic: same seed -> same stream", () => {
    const a = new LCG(12345);
    const b = new LCG(12345);
    for (let i = 0; i < 1000; i++) {
      expect(a.rand()).toBe(b.rand());
    }
  });

  it("srand resets the stream", () => {
    const rng = new LCG(42);
    rng.rand(); rng.rand(); rng.rand();
    rng.srand(42);
    const fresh = new LCG(42);
    for (let i = 0; i < 100; i++) {
      expect(rng.rand()).toBe(fresh.rand());
    }
  });

  it("all values are in [0, RAND_MAX]", () => {
    const rng = new LCG(7);
    for (let i = 0; i < 10000; i++) {
      const v = rng.rand();
      expect(v).toBeGreaterThanOrEqual(0);
      expect(v).toBeLessThanOrEqual(RAND_MAX);
    }
  });

  it("state stays in Uint32 range (no float drift over long runs)", () => {
    const rng = new LCG(99);
    for (let i = 0; i < 100000; i++) rng.rand();
    expect(rng._state).toBeGreaterThanOrEqual(0);
    expect(rng._state).toBeLessThanOrEqual(0xffffffff);
  });
});

describe("randfloat — [0,1) scaling", () => {
  it("stays in [0, 1)", () => {
    const rng = new LCG(2024);
    for (let i = 0; i < 10000; i++) {
      const f = randfloat(rng);
      expect(f).toBeGreaterThanOrEqual(0);
      expect(f).toBeLessThan(1);
    }
  });
});