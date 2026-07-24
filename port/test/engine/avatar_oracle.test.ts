/**
 * Oracle-golden test for avatar — diffs the TS port's
 * `GetBodyFromEmotion` output (chosen body/face/torso index + m_last* state)
 * against the frozen C++ oracle dump `oracle/avatar/avatar.golden.json`.
 *
 * The golden is produced by `OracleHarness.exe --avatar-pose` (new in
 * Phase 4) running the SAME probe battery + synthetic avatar data as
 * `port/src/engine/avatar_dump.ts`. This is the differential oracle check
 * for the emotion→pose selection (plan doc Tier-1 #2).
 *
 * Until the first CI run freezes `avatar.golden.json`, this test SKIPS
 * (the golden file doesn't exist locally). After the first freeze, commit
 * the golden and this test becomes the referee.
 */
import { describe, it, expect } from "vitest";
import { existsSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { dumpAvatarProbes } from "../../src/engine/avatar_dump.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
const GOLDEN_PATH = resolve(__dirname, "../../../oracle/avatar/avatar.golden.json");

const hasGolden = existsSync(GOLDEN_PATH);

describe("avatar oracle golden (differential)", () => {
  it.skipIf(!hasGolden)("TS port matches oracle avatar.golden.json for every probe", () => {
    const golden = JSON.parse(readFileSync(GOLDEN_PATH, "utf8")) as {
      simple: {
        input: { emotion: string; intensity: string };
        bodyIndex: number;
        m_lastBody: number;
      }[];
      complex: {
        input: { emotion: string; intensity: string };
        faceIndex: number;
        torsoIndex: number;
        m_lastFace: number;
        m_lastTorso: number;
      }[];
    };

    const tsDump = dumpAvatarProbes();

    // --- Simple avatar ---
    expect(tsDump.simple.length).toBe(golden.simple.length);
    for (let i = 0; i < golden.simple.length; i++) {
      const g = golden.simple[i];
      const t = tsDump.simple[i];
      // input floats (parsed-double Object.is — the oracle emits %.17g strings)
      for (const field of ["emotion", "intensity"] as const) {
        const tv = parseFloat(t.input[field]);
        const gv = parseFloat(g.input[field]);
        if (!Object.is(tv, gv)) {
          throw new Error(
            `simple[${i}].input.${field}: ts=${t.input[field]} golden=${g.input[field]}`,
          );
        }
      }
      if (t.bodyIndex !== g.bodyIndex) {
        throw new Error(
          `simple[${i}].bodyIndex: ts=${t.bodyIndex} golden=${g.bodyIndex}`,
        );
      }
      if (t.m_lastBody !== g.m_lastBody) {
        throw new Error(
          `simple[${i}].m_lastBody: ts=${t.m_lastBody} golden=${g.m_lastBody}`,
        );
      }
    }

    // --- Complex avatar ---
    expect(tsDump.complex.length).toBe(golden.complex.length);
    for (let i = 0; i < golden.complex.length; i++) {
      const g = golden.complex[i];
      const t = tsDump.complex[i];
      for (const field of ["emotion", "intensity"] as const) {
        const tv = parseFloat(t.input[field]);
        const gv = parseFloat(g.input[field]);
        if (!Object.is(tv, gv)) {
          throw new Error(
            `complex[${i}].input.${field}: ts=${t.input[field]} golden=${g.input[field]}`,
          );
        }
      }
      if (t.faceIndex !== g.faceIndex) {
        throw new Error(
          `complex[${i}].faceIndex: ts=${t.faceIndex} golden=${g.faceIndex}`,
        );
      }
      if (t.torsoIndex !== g.torsoIndex) {
        throw new Error(
          `complex[${i}].torsoIndex: ts=${t.torsoIndex} golden=${g.torsoIndex}`,
        );
      }
      if (t.m_lastFace !== g.m_lastFace) {
        throw new Error(
          `complex[${i}].m_lastFace: ts=${t.m_lastFace} golden=${g.m_lastFace}`,
        );
      }
      if (t.m_lastTorso !== g.m_lastTorso) {
        throw new Error(
          `complex[${i}].m_lastTorso: ts=${t.m_lastTorso} golden=${g.m_lastTorso}`,
        );
      }
    }
  });

  it("the probe battery is non-empty (sanity for the golden freeze)", () => {
    const dump = dumpAvatarProbes();
    expect(dump.simple.length).toBeGreaterThan(5);
    expect(dump.complex.length).toBeGreaterThan(5);
    for (const p of dump.simple) {
      expect(typeof p.input.emotion).toBe("string");
      expect(typeof p.bodyIndex).toBe("number");
      expect(typeof p.m_lastBody).toBe("number");
    }
  });
});
