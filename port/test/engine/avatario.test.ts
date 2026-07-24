/**
 * Golden tests for the avatario port — Tier-1 #3 (emotion quantization).
 *
 * The oracle corpus doesn't yet have an avatario dump mode (Phase 4 work),
 * so these goldens are HAND-COMPUTED from the C source
 * (avatario.cpp:45-98) and the frozen v2.5 emotion constants. The oracle
 * differential test (`avatario_oracle.test.ts`) will close this gap when
 * the C++ `--avatario` dump mode lands.
 *
 * Round-trip property: every known emotion at intensity 1.0 must survive
 * `EmotionToBytes` → `BytesToEmotion` with the same bit pattern (emotion
 * and intensity). The test below pins this for the whole table.
 */
import { describe, it, expect } from "vitest";
import { CEmotion } from "../../src/core/emotionopts.js";
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
} from "../../src/core/emotion.js";
import {
  ByteToIndex,
  BytesToEmotion,
  EmotionToBytes,
  EmotionToFloat,
  IndexToByte,
  emFloats,
} from "../../src/engine/avatario.js";

describe("emFloats[] table (avatario.cpp:45-64)", () => {
  it("has 18 entries: 0.0 + 8 directional + NEUTRAL + 8 special", () => {
    expect(emFloats.length).toBe(18);
    expect(emFloats[0]).toBe(0.0);
    expect(emFloats[9]).toBe(EM_NEUTRAL); // == 0.0
  });
  it("directional emotions sit on the wheel at k*2*PI/8", () => {
    // Index 1 = EM_HAPPY = (0*2*PI/8) -> 0; wait — EM_HAPPY is 0 * 2*PI/8 = 0.
    // So emFloats[1] = EM_HAPPY = 0, emFloats[2] = EM_COY = 2*PI/8, etc.
    expect(emFloats[1]).toBe(EM_HAPPY);
    expect(emFloats[2]).toBe(EM_COY);
    expect(emFloats[3]).toBe(EM_BORED);
    expect(emFloats[4]).toBe(EM_SCARED);
    expect(emFloats[5]).toBe(EM_SAD);
    expect(emFloats[6]).toBe(EM_ANGRY);
    expect(emFloats[7]).toBe(EM_SHOUT);
    expect(emFloats[8]).toBe(EM_LAUGH);
  });
  it("special (non-directional) emotions are sentinel floats 1001-1008", () => {
    expect(emFloats[10]).toBe(EM_WAVE);
    expect(emFloats[11]).toBe(EM_POINTOTHER);
    expect(emFloats[12]).toBe(EM_POINTSELF);
    expect(emFloats[13]).toBe(EM_DOUBLEPOINT);
    expect(emFloats[14]).toBe(EM_SHRUG);
    expect(emFloats[15]).toBe(EM_3QRWALK);
    expect(emFloats[16]).toBe(EM_SIDEWALK);
    expect(emFloats[17]).toBe(EM_3QFWALK);
  });
});

describe("IndexToByte / ByteToIndex (protsupp.cpp:1023-1032)", () => {
  it("IndexToByte adds '0' (0x30)", () => {
    expect(IndexToByte(0)).toBe(0x30);
    expect(IndexToByte(1)).toBe(0x31);
    expect(IndexToByte(9)).toBe(0x39);
    expect(IndexToByte(10)).toBe(0x3a); // ':'
    expect(IndexToByte(16)).toBe(0x40);
  });
  it("ByteToIndex subtracts '0'", () => {
    expect(ByteToIndex(0x30)).toBe(0);
    expect(ByteToIndex(0x39)).toBe(9);
    expect(ByteToIndex(0x3a)).toBe(10);
    expect(ByteToIndex(0x40)).toBe(16);
  });
  it("round-trip: ByteToIndex(IndexToByte(x)) === x for in-range values", () => {
    for (let i = 0; i < 17; i++) {
      expect(ByteToIndex(IndexToByte(i))).toBe(i);
    }
  });
});

describe("EmotionToBytes — wire encode (avatario.cpp:69-81)", () => {
  it("encodes emotion 0.0 to emVal=1 (first match), intensity 1.0 -> ':' (0x3a)", () => {
    // emFloats[1] = EM_HAPPY = 0.0 (EM_HAPPY = 0*2*PI/8 = 0.0). Search starts
    // at i=1 and stops on FIRST match, so 0.0 -> emVal=1, NOT emVal=9.
    // emFloats[9] = EM_NEUTRAL = 0.0 is the SECOND match; never reached.
    const em = new CEmotion(1.0, 0.0);
    const { emotion, intensity } = EmotionToBytes(em);
    expect(emotion).toBe(0x31); // IndexToByte(1) = '1'
    expect(intensity).toBe(0x3a); // IndexToByte(10) = ':'
  });
  it("encodes EM_HAPPY (0.0) to emVal=1 ('1' = 0x31)", () => {
    // Same: emFloats[1] = EM_HAPPY = 0.0, first match wins.
    const em = new CEmotion(1.0, EM_HAPPY);
    const { emotion } = EmotionToBytes(em);
    expect(emotion).toBe(0x31); // IndexToByte(1) = '1'
  });
  it("encodes EM_SHOUT (a directional) to its table index", () => {
    const em = new CEmotion(1.0, EM_SHOUT);
    // emFloats[7] = EM_SHOUT
    const { emotion } = EmotionToBytes(em);
    expect(emotion).toBe(0x37); // IndexToByte(7) = '7'
  });
  it("encodes EM_WAVE (sentinel 1001) to emVal=10", () => {
    const em = new CEmotion(1.0, EM_WAVE);
    const { emotion } = EmotionToBytes(em);
    expect(emotion).toBe(0x3a); // IndexToByte(10) = ':'
  });
  it("out-of-range emotion falls back to emVal=9 (NEUTRAL)", () => {
    const em = new CEmotion(1.0, 999.0);
    const { emotion } = EmotionToBytes(em);
    expect(emotion).toBe(0x39); // IndexToByte(9) = '9'
  });
  it("intensity truncation: (BYTE)(x * 10) — NOT ROUND", () => {
    // 0.3 * 10 = 3.0 -> BYTE 3 -> IndexToByte(3) = 0x33
    let em = new CEmotion(0.3, EM_HAPPY);
    expect(EmotionToBytes(em).intensity).toBe(0x33);
    // 0.5 * 10 = 5.0 -> 5
    em = new CEmotion(0.5, EM_HAPPY);
    expect(EmotionToBytes(em).intensity).toBe(0x35);
    // 1.5 * 10 = 15.0 -> 15 -> IndexToByte(15) = 0x3f
    em = new CEmotion(1.5, EM_HAPPY);
    expect(EmotionToBytes(em).intensity).toBe(0x3f);
    // 0.0 -> 0 -> IndexToByte(0) = 0x30
    em = new CEmotion(0.0, EM_HAPPY);
    expect(EmotionToBytes(em).intensity).toBe(0x30);
  });
});

describe("BytesToEmotion — wire decode (avatario.cpp:83-87)", () => {
  it("out-of-range emIndex -> NEUTRAL (0.0)", () => {
    const em = new CEmotion();
    BytesToEmotion(em, 200, 10);
    expect(em.m_emotion).toBe(EM_NEUTRAL);
  });
  it("in-range emIndex -> emFloats[emIndex]", () => {
    const em = new CEmotion();
    BytesToEmotion(em, 7, 10); // emFloats[7] = EM_SHOUT
    expect(em.m_emotion).toBe(EM_SHOUT);
    BytesToEmotion(em, 10, 5); // emFloats[10] = EM_WAVE
    expect(em.m_emotion).toBe(EM_WAVE);
  });
  it("intensity is inIndex / 10.0 (as float)", () => {
    const em = new CEmotion();
    BytesToEmotion(em, 1, 0);
    expect(em.m_intensity).toBe(0.0);
    BytesToEmotion(em, 1, 5);
    expect(em.m_intensity).toBe(0.5);
    BytesToEmotion(em, 1, 10);
    expect(em.m_intensity).toBe(1.0);
  });
});

describe("round-trip: EmotionToBytes -> BytesToEmotion is identity for known emotions", () => {
  // Every emFloats[] entry at intensity 1.0 must round-trip. Helper to keep
  // each test's per-iteration `e` bound cleanly (avoiding any for-closure
  // surprises in vitest's test-name expansion).
  function checkRoundTrip(e: number) {
    const emIn = new CEmotion(1.0, e);
    const { emotion: eByte, intensity: iByte } = EmotionToBytes(emIn);
    const emOut = new CEmotion();
    // The wire form is the ASCII bytes; the decode sees the IndexToByte
    // output, so we apply ByteToIndex to recover the table indices.
    BytesToEmotion(emOut, ByteToIndex(eByte), ByteToIndex(iByte));
    // The emotion MUST survive the round-trip bit-exact.
    expect(emOut.m_emotion).toBe(e);
    // Intensity 1.0 -> inVal 10 -> ByteToIndex(':' = 0x3a) = 10 -> 10/10 = 1.0.
    // Fround the comparison to match the float32 bit pattern the oracle emits.
    expect(Math.fround(emOut.m_intensity)).toBe(1.0);
  }
  function checkIntensityRoundTrip(intensity: number) {
    const emIn = new CEmotion(intensity, EM_HAPPY);
    const { emotion: eByte, intensity: iByte } = EmotionToBytes(emIn);
    const emOut = new CEmotion();
    BytesToEmotion(emOut, ByteToIndex(eByte), ByteToIndex(iByte));
    // The C `(BYTE)(intensity * 10)` truncates; BytesToEmotion recovers
    // `inIndex / 10.0`. For values where `intensity * 10` is exact (e.g.
    // 0.3 -> 3.0, 0.5 -> 5.0, 1.0 -> 10.0, 1.5 -> 15.0), the decode is
    // exact. For others, the truncation is lossy.
    const inVal = Math.trunc(intensity * 10);
    const expected = Math.fround(inVal / 10.0);
    expect(Math.fround(emOut.m_intensity)).toBe(expected);
  }
  it("emotion=0.0 round-trips", () => checkRoundTrip(0.0));
  it("emotion=EM_HAPPY round-trips", () => checkRoundTrip(EM_HAPPY));
  it("emotion=EM_COY round-trips", () => checkRoundTrip(EM_COY));
  it("emotion=EM_BORED round-trips", () => checkRoundTrip(EM_BORED));
  it("emotion=EM_SCARED round-trips", () => checkRoundTrip(EM_SCARED));
  it("emotion=EM_SAD round-trips", () => checkRoundTrip(EM_SAD));
  it("emotion=EM_ANGRY round-trips", () => checkRoundTrip(EM_ANGRY));
  it("emotion=EM_SHOUT round-trips", () => checkRoundTrip(EM_SHOUT));
  it("emotion=EM_LAUGH round-trips", () => checkRoundTrip(EM_LAUGH));
  it("emotion=EM_NEUTRAL round-trips", () => checkRoundTrip(0.0)); // EM_NEUTRAL alias
  it("emotion=EM_WAVE round-trips", () => checkRoundTrip(EM_WAVE));
  it("emotion=EM_POINTOTHER round-trips", () => checkRoundTrip(EM_POINTOTHER));
  it("emotion=EM_POINTSELF round-trips", () => checkRoundTrip(EM_POINTSELF));
  it("emotion=EM_DOUBLEPOINT round-trips", () => checkRoundTrip(EM_DOUBLEPOINT));
  it("emotion=EM_SHRUG round-trips", () => checkRoundTrip(EM_SHRUG));
  it("emotion=EM_3QRWALK round-trips", () => checkRoundTrip(EM_3QRWALK));
  it("emotion=EM_SIDEWALK round-trips", () => checkRoundTrip(EM_SIDEWALK));
  it("emotion=EM_3QFWALK round-trips", () => checkRoundTrip(EM_3QFWALK));
  it("intensity=0.0 round-trips", () => checkIntensityRoundTrip(0.0));
  it("intensity=0.3 round-trips", () => checkIntensityRoundTrip(0.3));
  it("intensity=0.5 round-trips", () => checkIntensityRoundTrip(0.5));
  it("intensity=0.8 round-trips", () => checkIntensityRoundTrip(0.8));
  it("intensity=1.5 round-trips", () => checkIntensityRoundTrip(1.5));
});

describe("EmotionToFloat (avatario.cpp:89-98)", () => {
  it("out-of-range index -> 0.0 (NOT EM_NEUTRAL — they're currently equal but distinct forms)", () => {
    expect(EmotionToFloat(-1)).toBe(0.0);
    expect(EmotionToFloat(99)).toBe(0.0);
    expect(EmotionToFloat(18)).toBe(0.0); // == emFloats.length
  });
  it("in-range index -> emFloats[index]", () => {
    expect(EmotionToFloat(0)).toBe(0.0);
    expect(EmotionToFloat(7)).toBe(EM_SHOUT);
    expect(EmotionToFloat(10)).toBe(EM_WAVE);
  });
});
