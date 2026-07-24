/**
 * Oracle-golden test for avatario — diffs the TS port's
 * `EmotionToBytes`/`BytesToEmotion` round-trip output against the frozen
 * C++ oracle dump `oracle/avatario/avatario.golden.json`.
 *
 * The golden is produced by `OracleHarness.exe --avatario` (new in Phase 4)
 * running the SAME probe battery as `port/src/engine/avatario_dump.ts`. This
 * closes the gap noted when avatario was first ported (only hand-computed
 * goldens existed). The hand-computed tests in `avatario.test.ts` remain as
 * the unit-level check; this is the differential oracle check.
 *
 * Until the first CI run freezes `avatario.golden.json`, this test SKIPS
 * (the golden file doesn't exist locally). After the first freeze, commit
 * the golden and this test becomes the referee.
 */
import { describe, it, expect } from "vitest";
import { existsSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { dumpAvatarioProbes } from "../../src/engine/avatario_dump.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
const GOLDEN_PATH = resolve(__dirname, "../../../oracle/avatario/avatario.golden.json");

const hasGolden = existsSync(GOLDEN_PATH);

describe("avatario oracle golden (differential)", () => {
  it.skipIf(!hasGolden)("TS port matches oracle avatario.golden.json for every probe", () => {
    const golden = JSON.parse(readFileSync(GOLDEN_PATH, "utf8")) as {
      emFloats: string[];
      probes: {
        input: { emotion: string; intensity: string };
        emVal: number;
        encoded: { emotion: number; intensity: number };
        decoded: { emotion: string; intensity: string };
      }[];
    };

    const tsDump = dumpAvatarioProbes();

    // emFloats[] table must match the oracle exactly.
    expect(tsDump.emFloats.length).toBe(golden.emFloats.length);
    for (let i = 0; i < golden.emFloats.length; i++) {
      const t = parseFloat(tsDump.emFloats[i]);
      const g = parseFloat(golden.emFloats[i]);
      if (!Object.is(t, g)) {
        throw new Error(
          `emFloats[${i}]: ts=${tsDump.emFloats[i]} golden=${golden.emFloats[i]}`,
        );
      }
    }

    // Probes must match exactly.
    expect(tsDump.probes.length).toBe(golden.probes.length);
    let mismatches = 0;
    const diffs: string[] = [];
    for (let i = 0; i < golden.probes.length; i++) {
      const g = golden.probes[i];
      const t = tsDump.probes[i];
      // emVal
      if (t.emVal !== g.emVal) {
        mismatches++;
        diffs.push(`probe[${i}].emVal: ts=${t.emVal} golden=${g.emVal}`);
      }
      // encoded bytes (integer equality)
      if (t.encoded.emotion !== g.encoded.emotion) {
        mismatches++;
        diffs.push(
          `probe[${i}].encoded.emotion: ts=${t.encoded.emotion} golden=${g.encoded.emotion}`,
        );
      }
      if (t.encoded.intensity !== g.encoded.intensity) {
        mismatches++;
        diffs.push(
          `probe[${i}].encoded.intensity: ts=${t.encoded.intensity} golden=${g.encoded.intensity}`,
        );
      }
      // Compare the four float fields (input.emotion/intensity + decoded.emotion/intensity)
      // via parsed-double Object.is (the oracle emits strings via %.17g).
      for (const field of [
        "input.emotion",
        "input.intensity",
        "decoded.emotion",
        "decoded.intensity",
      ] as const) {
        const [grp, key] = field.split(".") as ["input" | "decoded", "emotion" | "intensity"];
        const ts = parseFloat(t[grp][key]);
        const go = parseFloat(g[grp][key]);
        if (!Object.is(ts, go)) {
          mismatches++;
          diffs.push(
            `probe[${i}].${field}: ts=${t[grp][key]} golden=${g[grp][key]}`,
          );
        }
      }
    }
    if (mismatches > 0) {
      throw new Error(
        `avatario oracle golden mismatch: ${mismatches} diffs\n${diffs.slice(0, 15).join("\n")}`,
      );
    }
  });

  it("the probe battery is non-empty (sanity for the golden freeze)", () => {
    const dump = dumpAvatarioProbes();
    expect(dump.probes.length).toBeGreaterThan(15);
    expect(dump.emFloats.length).toBe(18);
    for (const p of dump.probes) {
      expect(typeof p.input.emotion).toBe("string");
      expect(typeof p.input.intensity).toBe("string");
      expect(typeof p.decoded.emotion).toBe("string");
      expect(typeof p.decoded.intensity).toBe("string");
    }
  });
});
