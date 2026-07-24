/**
 * Avatario probe dumper — the TS mirror of `CaptureAvatario` in
 * `oracle/harness/oracleharness.cpp`. Runs a fixed probe battery through
 * `EmotionToBytes` → `BytesToEmotion` and returns a JSON-identical
 * structure so the oracle golden test can diff the two byte-for-byte.
 *
 * KEEP THE PROBE LIST IN SYNC with `CaptureAvatario` in the C++ harness.
 * Both lists must contain the same inputs in the same order.
 *
 * Each probe has:
 *   - input.emotion: the source emotion (float, "%.17g" of the double)
 *   - input.intensity: the source intensity
 *   - encoded.emotion: the byte from IndexToByte(emVal) — the wire byte
 *   - encoded.intensity: the byte from IndexToByte(inVal) — the wire byte
 *   - decoded.emotion: the float back out of BytesToEmotion
 *   - decoded.intensity: the float back out of BytesToEmotion
 *   - emVal: the table index picked (or 9 NEUTRAL on no match)
 *
 * The round-trip check (decoded matches input) is asserted in the test,
 * not encoded here — the dump just records the data.
 */
import { CEmotion } from "../core/emotionopts.js";
import {
  EM_3QFWALK,
  EM_3QRWALK,
  EM_ANGRY,
  EM_BORED,
  EM_COY,
  EM_DOUBLEPOINT,
  EM_HAPPY,
  EM_LAUGH,
  EM_NEUTRAL,
  EM_POINTOTHER,
  EM_POINTSELF,
  EM_SAD,
  EM_SCARED,
  EM_SHOUT,
  EM_SHRUG,
  EM_SIDEWALK,
  EM_WAVE,
} from "../core/emotion.js";
import {
  BytesToEmotion,
  EmotionToBytes,
  emFloats,
} from "./avatario.js";

export interface AvatarioProbeResult {
  input: { emotion: string; intensity: string };
  emVal: number;
  encoded: { emotion: number; intensity: number };
  decoded: { emotion: string; intensity: string };
}
export interface AvatarioDump {
  emFloats: string[];
  probes: AvatarioProbeResult[];
}

// The probe battery — identical to the C++ CaptureAvatario probes[] array.
// Each entry is the (emotion, intensity) pair fed into EmotionToBytes.
const PROBES: readonly { emotion: number; intensity: number; note: string }[] = [
  // 17 known emotions at intensity 1.0 (cover the entire emFloats[] table).
  { emotion: 0.0, intensity: 1.0, note: "NEUTRAL via 0.0" },
  { emotion: EM_HAPPY, intensity: 1.0, note: "HAPPY" },
  { emotion: EM_COY, intensity: 1.0, note: "COY" },
  { emotion: EM_BORED, intensity: 1.0, note: "BORED" },
  { emotion: EM_SCARED, intensity: 1.0, note: "SCARED" },
  { emotion: EM_SAD, intensity: 1.0, note: "SAD" },
  { emotion: EM_ANGRY, intensity: 1.0, note: "ANGRY" },
  { emotion: EM_SHOUT, intensity: 1.0, note: "SHOUT" },
  { emotion: EM_LAUGH, intensity: 1.0, note: "LAUGH" },
  { emotion: EM_NEUTRAL, intensity: 1.0, note: "NEUTRAL via EM_NEUTRAL" },
  { emotion: EM_WAVE, intensity: 1.0, note: "WAVE" },
  { emotion: EM_POINTOTHER, intensity: 1.0, note: "POINTOTHER" },
  { emotion: EM_POINTSELF, intensity: 1.0, note: "POINTSELF" },
  { emotion: EM_DOUBLEPOINT, intensity: 1.0, note: "DOUBLEPOINT" },
  { emotion: EM_SHRUG, intensity: 1.0, note: "SHRUG" },
  { emotion: EM_3QRWALK, intensity: 1.0, note: "3QRWALK" },
  { emotion: EM_SIDEWALK, intensity: 1.0, note: "SIDEWALK" },
  { emotion: EM_3QFWALK, intensity: 1.0, note: "3QFWALK" },
  // Out-of-range emotion -> emVal stays at 9 (NEUTRAL).
  { emotion: 999.0, intensity: 1.0, note: "out-of-range -> NEUTRAL" },
  // Intensity scaling
  { emotion: EM_HAPPY, intensity: 0.0, note: "intensity=0 -> 0 byte" },
  { emotion: EM_HAPPY, intensity: 0.3, note: "intensity=0.3 -> 3 byte" },
  { emotion: EM_HAPPY, intensity: 0.5, note: "intensity=0.5 -> 5 byte" },
  { emotion: EM_HAPPY, intensity: 1.5, note: "intensity=1.5 -> 15 byte" },
];

export function dumpAvatarioProbes(): AvatarioDump {
  // Dump the emFloats[] table itself for oracle comparison.
  const emFloatsOut = emFloats.map((v) => String(v));

  const probes: AvatarioProbeResult[] = [];
  for (const probe of PROBES) {
    const emIn = new CEmotion(probe.intensity, probe.emotion);
    const { emotion: emotionByte, intensity: intensityByte } = EmotionToBytes(emIn);

    // Decode back to check the round-trip.
    const emOut = new CEmotion();
    // Pass the RAW table-index bytes (what C gets after the IndexToByte
    // round-trip in the wire form: IndexToByte(emVal) on encode, then
    // ByteToIndex(emotionByte) on decode gives back emVal). For the
    // round-trip we need to use ByteToIndex here.
    BytesToEmotion(
      emOut,
      emotionByte - 0x30,
      intensityByte - 0x30,
    );

    // Reconstruct the emVal the encoder picked. We do this independently
    // (the TS already knows — the test asserts it matches the input).
    // For the dump, we report it as a sanity field.
    let emVal = 9; // default
    const emInFround = Math.fround(probe.emotion);
    for (let i = 1; i < emFloats.length; i++) {
      if (emFloats[i] === emInFround) {
        emVal = i;
        break;
      }
    }

    probes.push({
      input: {
        emotion: String(probe.emotion),
        intensity: String(probe.intensity),
      },
      emVal,
      encoded: { emotion: emotionByte, intensity: intensityByte },
      decoded: {
        emotion: String(emOut.m_emotion),
        intensity: String(emOut.m_intensity),
      },
    });
  }
  return { emFloats: emFloatsOut, probes };
}
