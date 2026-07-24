/**
 * Port of `avatario.cpp` — emotion-wheel quantization (Tier-1 #3 in plan doc).
 *
 * Source: `v2.5-beta-1-modern/avatario.cpp:45-98` (the golden oracle tree;
 * the v1.0-pre-modern version is byte-identical for these routines — only
 * the old avb parser differs and is gone from v2.5).
 *
 * What this file ports:
 *   - `emFloats[]` — the 17-entry emotion-wheel table (index 0 = NEUTRAL=0.0,
 *     indices 1..16 = the 8 directional emotions + 8 sentinel emotions).
 *   - `IndexToByte` / `ByteToIndex` — ASCII-digit conversion of the table
 *     index (avatario.cpp:66-67 forward-declared, defined in protsupp.cpp:1023-1032;
 *     these are independent of the `SM2BM`/`BM2SM` mode maps). `IndexToByte`
 *     just does `byteIn + '0'`; `ByteToIndex` is `byteIn - '0'`. The protocol
 *     level uses these for the UDI emotion byte in the wire format
 *     (`protsupp.cpp` `#G…E…R M T…` blocks).
 *   - `EmotionToBytes(CEmotion, BYTE&, BYTE&)` — encode an emotion for the
 *     wire. Linear search `emFloats[1..N]` for an EXACT FLOAT MATCH against
 *     `em.m_emotion` (the table values are float32; the search is by bit
 *     pattern). Default emVal=9 (NEUTRAL) if no match. Intensity is encoded
 *     as `(BYTE)(intensity * 10)` — truncation (NOT ROUND) of the float product.
 *     Both bytes then go through `IndexToByte` (just adds `'0'`) for the wire.
 *   - `BytesToEmotion(CEmotion&, BYTE, BYTE)` — decode the wire bytes back.
 *     Out-of-range emIndex -> NEUTRAL. Intensity = `(float)(inIndex / 10.0)`.
 *   - `EmotionToFloat(int index)` — out-of-range -> 0.0, else `emFloats[index]`.
 *     This is the form consumed by `GetBodyFromEmotion` when it needs to
 *     match a wire-emotion byte back to a table entry.
 *
 * **This is the protocol + replay boundary** — the UDI wire format and the
 * `.ccc` transcript both pass emotions through this encoding, so any drift
 * here cascades into every other surface. The oracle golden freezes the
 * round-trip on a fixed battery of probe inputs.
 *
 * RULEBOOK notes:
 *   - §1.1 Float-cast via `Math.fround`: the `emFloats[]` entries are
 *     float32 bit patterns (`(float)(k * 2 * PI / 8)` etc.); the
 *     exact-match search in `EmotionToBytes` requires both sides to be
 *     fround'd. `em.m_emotion` from the engine is also float; in the port
 *     we fround it at the comparison so the test reproduces the C behavior.
 *   - §1.2 PI is tree-dependent: the directional emotion constants use
 *     v2.5 full-precision PI; `emFloats[]` does the same. The port's
 *     `emotion.ts` exports those constants already-fround.
 *   - The `(BYTE)(intensity * 10)` cast in `EmotionToBytes` is **truncation**,
 *     not rounding (C `(BYTE)x` truncates toward zero). Use `Math.trunc` /
 *     `| 0`, not `ROUND` (RULEBOOK §1 distinguishes ROUND vs INT_CAST).
 *   - The `(float)(inIndex / 10.0)` in `BytesToEmotion` is a `double / 10.0`
 *     then cast to float — use `Math.fround(inIndex / 10.0)` to match the
 *     bit pattern the oracle emits.
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
import { INT_CAST } from "../core/numeric.js";

// ---------------------------------------------------------------------------
// emFloats[] — avatario.cpp:45-64. The 17-entry emotion-wheel table. Index 0
// is always 0.0 (NEUTRAL); indices 1..16 are the named emotions. The C casts
// each value to `(float)`; the port's emotion constants are already fround,
// so a direct reference is bit-identical.
// ---------------------------------------------------------------------------
export const emFloats: readonly number[] = Object.freeze([
  /*  0 */ 0.0,
  /*  1 */ EM_HAPPY,
  /*  2 */ EM_COY,
  /*  3 */ EM_BORED,
  /*  4 */ EM_SCARED,
  /*  5 */ EM_SAD,
  /*  6 */ EM_ANGRY,
  /*  7 */ EM_SHOUT,
  /*  8 */ EM_LAUGH,
  /*  9 */ EM_NEUTRAL, // == 0.0
  /* 10 */ EM_WAVE,
  /* 11 */ EM_POINTOTHER,
  /* 12 */ EM_POINTSELF,
  /* 13 */ EM_DOUBLEPOINT,
  /* 14 */ EM_SHRUG,
  /* 15 */ EM_3QRWALK,
  /* 16 */ EM_SIDEWALK,
  /* 17 */ EM_3QFWALK,
]);

// ---------------------------------------------------------------------------
// IndexToByte / ByteToIndex — avatario.cpp:66-67 (forward-declared), defined
// in protsupp.cpp:1023-1032. Plain ASCII-digit conversion. The +'0' / -'0'
// form matches the byte range the C engine uses: emVal=0..16 becomes the
// ASCII chars 0x30..0x40, inVal=0..10 becomes 0x30..0x3A.
// ---------------------------------------------------------------------------
export function IndexToByte(byteIn: number): number {
  return (byteIn + 0x30) & 0xff;
}

export function ByteToIndex(byteIn: number): number {
  return (byteIn - 0x30) & 0xff;
}

// ---------------------------------------------------------------------------
// EmotionToBytes — avatario.cpp:69-81. The wire encode.
// Linear search emFloats[1..N] for emFloats[i] === em.m_emotion (exact float
// match — the table is float32, so fround em.m_emotion at the comparison).
// Default emVal = 9 (NEUTRAL index) on no match.
// Intensity: (BYTE)(em.m_intensity * 10) — truncation toward zero (INT_CAST).
// Then both bytes go through IndexToByte for the wire form.
// ---------------------------------------------------------------------------
export function EmotionToBytes(
  em: CEmotion,
): { emotion: number; intensity: number } {
  let emVal = 9; // neutral index — always safe
  const n = emFloats.length;
  // Fround the input for the float32 comparison (the table is float32).
  const emFround = Math.fround(em.m_emotion);
  for (let i = 1; i < n; i++) {
    if (emFloats[i] === emFround) {
      emVal = i;
      break;
    }
  }
  // (BYTE)(intensity * 10) — C truncates toward zero. INT_CAST uses
  // Math.trunc | 0, which clamps to Int32. The result is masked to byte
  // by the C BYTE cast (C BYTE = unsigned 8-bit); we apply & 0xff to match.
  const inVal = INT_CAST(em.m_intensity * 10) & 0xff;
  return { emotion: IndexToByte(emVal), intensity: IndexToByte(inVal) };
}

// ---------------------------------------------------------------------------
// BytesToEmotion — avatario.cpp:83-87. The wire decode.
// Out-of-range emIndex -> NEUTRAL. Intensity = (float)(inIndex / 10.0) — the
// C cast is to `float` so we fround the double result.
// ---------------------------------------------------------------------------
export function BytesToEmotion(
  em: CEmotion,
  emIndex: number,
  inIndex: number,
): void {
  if (emIndex < 0 || emIndex >= emFloats.length) {
    em.m_emotion = EM_NEUTRAL;
  } else {
    em.m_emotion = emFloats[emIndex];
  }
  // (float)(inIndex / 10.0): JS computes inIndex/10.0 as a double, then
  // we fround to match the float32 bit pattern the oracle emits.
  em.m_intensity = Math.fround(inIndex / 10.0);
}

// ---------------------------------------------------------------------------
// EmotionToFloat — avatario.cpp:89-98. Lookup helper. Out-of-range index
// returns 0.0 (NOT EM_NEUTRAL — the C explicitly returns 0.0, not emFloats[9]
// which happens to equal 0.0 here, but a future change to the table could
// decouple them).
// ---------------------------------------------------------------------------
export function EmotionToFloat(index: number): number {
  if (index < 0 || index >= emFloats.length) {
    return 0.0;
  }
  return emFloats[index];
}
