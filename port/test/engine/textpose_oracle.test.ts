/**
 * Oracle-golden test for textpose — diffs the TS port's `GetEmotionsFromString`
 * output against the frozen C++ oracle dump `oracle/textpose/textpose.golden.json`.
 *
 * The golden is produced by `OracleHarness.exe --textpose` (new in Phase 4)
 * running the SAME probe battery as `port/src/engine/textpose_dump.ts`. This
 * closes the gap noted when textpose was first ported (only hand-computed
 * goldens existed). The hand-computed tests in `textpose.test.ts` remain as
 * the unit-level check; this is the differential oracle check.
 *
 * Until the first CI run freezes `textpose.golden.json`, this test SKIPS
 * (the golden file doesn't exist locally). After the first freeze, commit the
 * golden and this test becomes the referee.
 */
import { describe, it, expect } from "vitest";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { hasGoldenOrThrowInCI } from "../helpers/golden.js";
import { GetEmotionsFromString, InitializeEmotionRules } from "../../src/engine/textpose.js";
import { CEmotionOpts } from "../../src/core/emotionopts.js";
import { dumpTextposeProbes } from "../../src/engine/textpose_dump.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
const GOLDEN_PATH = resolve(__dirname, "../../../oracle/textpose/textpose.golden.json");

// The emotion constants are floats; the oracle emits them as "%.17g" strings.
// The TS port computes them from PI=3.14159 the same way, so they should be
// bit-identical doubles. The comparison parses both sides and uses Object.is
// for strict float-equality (any drift is a real divergence). See the
// per-opt comparison in the test below.
void 0; // (helper removed; comparison is inline)

const hasGolden = hasGoldenOrThrowInCI(GOLDEN_PATH);

describe("textpose oracle golden (differential)", () => {
  it.skipIf(!hasGolden)("TS port matches oracle textpose.golden.json for every probe", () => {
    InitializeEmotionRules();
    const golden = JSON.parse(readFileSync(GOLDEN_PATH, "utf8")) as {
      probes: { text: string; opts: { emotion: string; intensity: string; priority: number }[] }[];
    };

    const tsDump = dumpTextposeProbes(GetEmotionsFromString, InitializeEmotionRules);

    expect(tsDump.probes.length).toBe(golden.probes.length);
    let mismatches = 0;
    const diffs: string[] = [];
    for (let i = 0; i < golden.probes.length; i++) {
      const g = golden.probes[i];
      const t = tsDump.probes[i];
      if (t.text !== g.text) {
        mismatches++;
        diffs.push(`probe[${i}] text: ts=${JSON.stringify(t.text)} golden=${JSON.stringify(g.text)}`);
        continue;
      }
      if (t.opts.length !== g.opts.length) {
        mismatches++;
        diffs.push(
          `probe[${i}] (${JSON.stringify(g.text)}): ts has ${t.opts.length} opts, golden has ${g.opts.length}`,
        );
        continue;
      }
      for (let j = 0; j < g.opts.length; j++) {
        const ge = g.opts[j];
        const te = t.opts[j];
        // Compare as parsed doubles (the oracle emits strings via %.17g;
        // the TS dump emits strings via Number.toString()). Both are shortest
        // round-trip for the same double, so parse both and compare with a
        // strict float-equality (Object.is) — any drift is a real divergence.
        const ge_emo = parseFloat(ge.emotion);
        const te_emo = parseFloat(te.emotion);
        const ge_int = parseFloat(ge.intensity);
        const te_int = parseFloat(te.intensity);
        if (!Object.is(te_emo, ge_emo)) {
          mismatches++;
          diffs.push(`probe[${i}] opt[${j}] emotion: ts=${te.emotion} golden=${ge.emotion}`);
        }
        if (!Object.is(te_int, ge_int)) {
          mismatches++;
          diffs.push(`probe[${i}] opt[${j}] intensity: ts=${te.intensity} golden=${ge.intensity}`);
        }
        if (te.priority !== ge.priority) {
          mismatches++;
          diffs.push(`probe[${i}] opt[${j}] priority: ts=${te.priority} golden=${ge.priority}`);
        }
      }
    }
    if (mismatches > 0) {
      throw new Error(`textpose oracle golden mismatch: ${mismatches} diffs\n${diffs.slice(0, 15).join("\n")}`);
    }
  });

  it("the probe battery is non-empty (sanity for the golden freeze)", () => {
    const dump = dumpTextposeProbes(GetEmotionsFromString, InitializeEmotionRules);
    expect(dump.probes.length).toBeGreaterThan(20);
    // Every probe has a text and an opts array (possibly empty).
    for (const p of dump.probes) {
      expect(typeof p.text).toBe("string");
      expect(Array.isArray(p.opts)).toBe(true);
    }
  });
});

// Keep CEmotionOpts imported for type-checking even if the golden test skips.
void CEmotionOpts;