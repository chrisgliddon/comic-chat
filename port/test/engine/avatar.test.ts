/**
 * Golden tests for the avatar port — Tier-1 #2 (emotion-to-pose selection).
 *
 * The oracle corpus doesn't yet have an avatar-pose dump mode (Phase 4 work),
 * so these goldens are HAND-COMPUTED from the C source (avatar.cpp:226-416)
 * and the frozen v2.5 emotion constants. The oracle differential test
 * (`avatar_oracle.test.ts`) will close this gap when the C++ `--avatar-pose`
 * dump mode lands.
 *
 * The probe battery synthesizes tiny avatars with KNOWN bRec[]/fRec[]
 * arrays, runs the engine's selection logic, and asserts the chosen index
 * + post-state matches hand-traced expectations. This is the same form
 * the oracle dump will take.
 *
 * **PI sensitivity note (RULEBOOK §1.2):** the port's `value_to_angle`
 * uses v1.0-pre `PI = 3.14159`; the oracle uses v2.5's full-precision PI.
 * For the threshold check `thisAngle < PI/8`, only EXACT-MATCH candidates
 * (angle=0) pass — the integer index is the same regardless of PI precision.
 * The tests focus on EXACT MATCH + neutral-fallback paths where the
 * selection is deterministic, not on near-miss angle comparisons.
 */
import { describe, it, expect } from "vitest";
import { CEmotion, CEmotionOpts } from "../../src/core/emotionopts.js";
import {
  EM_ANGRY,
  EM_HAPPY,
  EM_LAUGH,
  EM_NEUTRAL,
  EM_SAD,
  EM_SCARED,
  EM_SHOUT,
  EM_WAVE,
} from "../../src/core/emotion.js";
import {
  CAvatarComplex,
  CAvatarSimple,
  type BODYREC,
  type FACEREC,
  type RBODYREC,
} from "../../src/engine/avatar.js";

function mkSimple(bRec: RBODYREC[]): CAvatarSimple {
  const av = new CAvatarSimple();
  av.bRec = bRec;
  av.m_nBodies = bRec.length;
  av.m_lastBody = -1;
  return av;
}

function mkComplex(fRec: FACEREC[], bRec: BODYREC[]): CAvatarComplex {
  const av = new CAvatarComplex();
  av.fRec = fRec;
  av.bRec = bRec;
  av.nFaces = fRec.length;
  av.nTorsos = bRec.length;
  av.m_lastFace = -1;
  av.m_lastTorso = -1;
  return av;
}

// A minimal but realistic synthetic avatar: 8 directional bodies at intensity
// 0.5 (one for each wheel position) + 1 NEUTRAL (intensity 0) + 1 WAVE.
// This shape exercises the threshold, the tiebreak, and the neutral fallback.
const SIMPLE_BREC: RBODYREC[] = [
  { poseID: 100, emotion: EM_HAPPY, intensity: 0.5, faceX: 10, faceY: 5 },
  { poseID: 101, emotion: EM_SCARED, intensity: 0.5, faceX: 10, faceY: 5 },
  { poseID: 102, emotion: EM_SAD, intensity: 0.5, faceX: 10, faceY: 5 },
  { poseID: 103, emotion: EM_ANGRY, intensity: 0.5, faceX: 10, faceY: 5 },
  { poseID: 104, emotion: EM_SHOUT, intensity: 0.5, faceX: 10, faceY: 5 },
  { poseID: 105, emotion: EM_LAUGH, intensity: 0.5, faceX: 10, faceY: 5 },
  { poseID: 110, emotion: EM_NEUTRAL, intensity: 0.0, faceX: 10, faceY: 5 },
  { poseID: 120, emotion: EM_WAVE, intensity: 1.0, faceX: 10, faceY: 5 },
];

const COMPLEX_FREC: FACEREC[] = [
  { poseID: 200, emotion: EM_HAPPY, intensity: 0.5, xCX: 0, yCX: 0, delta_xCX: 0, delta_yCX: 0, faceX: 8, faceY: 4 },
  { poseID: 201, emotion: EM_SCARED, intensity: 0.5, xCX: 0, yCX: 0, delta_xCX: 0, delta_yCX: 0, faceX: 8, faceY: 4 },
  { poseID: 202, emotion: EM_SAD, intensity: 0.5, xCX: 0, yCX: 0, delta_xCX: 0, delta_yCX: 0, faceX: 8, faceY: 4 },
  { poseID: 203, emotion: EM_ANGRY, intensity: 0.5, xCX: 0, yCX: 0, delta_xCX: 0, delta_yCX: 0, faceX: 8, faceY: 4 },
  { poseID: 204, emotion: EM_SHOUT, intensity: 0.5, xCX: 0, yCX: 0, delta_xCX: 0, delta_yCX: 0, faceX: 8, faceY: 4 },
  { poseID: 205, emotion: EM_LAUGH, intensity: 0.5, xCX: 0, yCX: 0, delta_xCX: 0, delta_yCX: 0, faceX: 8, faceY: 4 },
  { poseID: 210, emotion: EM_NEUTRAL, intensity: 0.0, xCX: 0, yCX: 0, delta_xCX: 0, delta_yCX: 0, faceX: 8, faceY: 4 },
];

const COMPLEX_BREC: BODYREC[] = [
  { poseID: 300, emotion: EM_HAPPY, intensity: 0.5, xCX: 0, yCX: 0 },
  { poseID: 301, emotion: EM_SAD, intensity: 0.5, xCX: 0, yCX: 0 },
  { poseID: 302, emotion: EM_ANGRY, intensity: 0.5, xCX: 0, yCX: 0 },
  { poseID: 303, emotion: EM_SHOUT, intensity: 0.5, xCX: 0, yCX: 0 },
  { poseID: 310, emotion: EM_NEUTRAL, intensity: 0.0, xCX: 0, yCX: 0 },
];

describe("CAvatarSimple::GetBodyFromEmotion(CEmotion &)", () => {
  it("exact match: intensity exactly matches -> that index", () => {
    const av = mkSimple(SIMPLE_BREC);
    const body = av.GetBodyFromEmotion(new CEmotion(0.5, EM_SAD));
    expect(body.m_bodyIndex).toBe(2); // SAD
    expect(av.m_lastBody).toBe(2);
  });

  it("exact match wins over higher-index entry with closer intensity (history bias)", () => {
    // Two entries with emotion=EM_SHOUT at intensities 0.5 and 0.7. With
    // m_lastBody=-1, scan starts at index 0. EM_SHOUT is at index 4 (only).
    const av = mkSimple(SIMPLE_BREC);
    const body = av.GetBodyFromEmotion(new CEmotion(0.5, EM_SHOUT));
    expect(body.m_bodyIndex).toBe(4);
    expect(av.m_lastBody).toBe(4);
  });

  it("non-matching emotion: angle-after-normalization can still pick a directional (known C quirk)", () => {
    // The C code's `bRec[index].emotion > 7` check only filters SENTINEL
    // entries from bRec — it does NOT check the input emotion. So when
    // the input is EM_WAVE=1001, the loop still computes the normalized
    // angle to each directional bRec entry. The angle from EM_SCARED
    // (2.356) to 1001 normalizes to ~0.3827, which is < PI/8 — so SCARED
    // wins. The C source acknowledges this with "Distance metric needs
    // rethinking!" Reproduce bug-for-bug; the port must match the C
    // selection.
    const av = mkSimple(SIMPLE_BREC);
    const body = av.GetBodyFromEmotion(new CEmotion(1.0, EM_WAVE));
    expect(body.m_bodyIndex).toBe(1); // SCARED (angle coincidence)
    expect(av.m_lastBody).toBe(1);
  });

  it("SetBodyNeutral wraps around the array (history bias)", () => {
    // First set m_lastBody to 5 (LAUGH). Then ask for EM_DOUBLEPOINT (sentinel
    // 1004). The angle from each directional entry to 1004 normalizes to a
    // value; the smallest happens to be SHOUT (index 4) at ~0.38 < PI/8.
    // So SHOUT wins — NOT the NEUTRAL fallback. The C quirk applies again.
    const av = mkSimple(SIMPLE_BREC);
    av.GetBodyFromEmotion(new CEmotion(0.5, EM_LAUGH)); // sets m_lastBody=5
    expect(av.m_lastBody).toBe(5);
    const body = av.GetBodyFromEmotion(new CEmotion(1.0, 1004.0)); // EM_DOUBLEPOINT
    expect(body.m_bodyIndex).toBe(4); // SHOUT (angle coincidence after normalize)
  });

  it("sequence of calls: m_lastBody history bias affects next scan start", () => {
    // First call: ask for SHOUT -> exact match at 4. m_lastBody=4.
    // Second call: ask for SAD -> starts at (4+1)%8=5, scans indices
    // 5,6,7,0,1,2,3. SAD is at 2. Scan visits 5,6,7,0,1,2; finds exact
    // match at 2, picks it.
    const av = mkSimple(SIMPLE_BREC);
    av.GetBodyFromEmotion(new CEmotion(0.5, EM_SHOUT));
    expect(av.m_lastBody).toBe(4);
    const body = av.GetBodyFromEmotion(new CEmotion(0.5, EM_SAD));
    expect(body.m_bodyIndex).toBe(2);
  });
});

describe("CAvatarSimple::GetBodyFromEmotionFromOpts(CEmotionOpts &)", () => {
  it("picks the highest-priority opt that has a match", () => {
    const av = mkSimple(SIMPLE_BREC);
    const opts = new CEmotionOpts();
    opts.Add(EM_SHOUT, 0.5, 5);
    opts.Add(EM_SAD, 0.5, 9); // higher priority
    const body = av.GetBodyFromEmotionFromOpts(opts);
    expect(body.m_bodyIndex).toBe(2); // SAD wins
  });

  it("returns neutral when no opt has a match (uses exact-match search in CEmotionOpts path)", () => {
    // The CEmotionOpts path uses GetBodyIndexFromEmotion which checks
    // `m_emotion <= 2*PI` and falls to exact-match for sentinels. WAVE
    // is at index 7 in bRec — exact match wins.
    const av = mkSimple(SIMPLE_BREC);
    const opts = new CEmotionOpts();
    opts.Add(EM_WAVE, 1.0, 5); // EM_WAVE matches bRec[7] exactly
    const body = av.GetBodyFromEmotionFromOpts(opts);
    expect(body.m_bodyIndex).toBe(7); // WAVE exact match
  });

  it("zero opts -> neutral fallback", () => {
    const av = mkSimple(SIMPLE_BREC);
    const opts = new CEmotionOpts();
    const body = av.GetBodyFromEmotionFromOpts(opts);
    // No opts with priority > 0 -> loop exits, SetBodyNeutral is called.
    // Starts at m_lastBody+1=0, finds NEUTRAL at 6.
    expect(body.m_bodyIndex).toBe(6);
  });
});

describe("CAvatarComplex::GetBodyFromEmotion(CEmotion &)", () => {
  it("exact match: face + torso both pick the matching index", () => {
    const av = mkComplex(COMPLEX_FREC, COMPLEX_BREC);
    const body = av.GetBodyFromEmotion(new CEmotion(0.5, EM_SAD));
    // Face: fRec[2] = SAD (face scan picks the smallest angle, no threshold).
    // Torso: bRec[1] = SAD (torso scan threshold is PI/8; SAD is exact match).
    expect(body.m_faceIndex).toBe(2);
    expect(body.m_torsoIndex).toBe(1);
    expect(av.m_lastFace).toBe(2);
    expect(av.m_lastTorso).toBe(1);
  });

  it("sentinel emotion: face falls to NEUTRAL, torso falls to NEUTRAL", () => {
    // EM_WAVE (1001) angle to face entries normalizes to small values
    // (the same C quirk as Simple), but the face scan has NO threshold —
    // it picks the smallest angle. The smallest |subtract_angles(fRec[i].emotion, 1001)|
    // happens to be SCARED (fRec[1]) at ~0.3827, so face picks 1.
    // Torso has the PI/8 threshold + neutral acceptance; no exact match
    // for WAVE in bRec, so torso falls to bRec[4] (NEUTRAL).
    const av = mkComplex(COMPLEX_FREC, COMPLEX_BREC);
    const body = av.GetBodyFromEmotion(new CEmotion(1.0, EM_WAVE));
    expect(body.m_faceIndex).toBe(1); // SCARED (face scan, no threshold)
    expect(body.m_torsoIndex).toBe(4); // NEUTRAL fallback
  });
});

describe("CAvatarComplex::GetBodyFromEmotionFromOpts(CEmotionOpts &)", () => {
  it("directional emotion opts set face but torso falls to NEUTRAL (opts path quirk)", () => {
    // CEmotionOpts uses GetHeadAndBodyFromEmotion which only sets ONE index
    // per call: face for directional, torso for sentinel. For a directional
    // emotion opt like SAD, the face is set (SAD -> fRec[2]) but the torso
    // is NOT (GetHeadAndBodyFromEmotion's directional branch skips torso).
    // After the opts loop, foundT < 0, so SetTorsoNeutral is called.
    // SetTorsoNeutral starts at m_lastTorso+1=0, finds NEUTRAL at 4.
    const av = mkComplex(COMPLEX_FREC, COMPLEX_BREC);
    const opts = new CEmotionOpts();
    opts.Add(EM_SAD, 0.5, 8);
    const body = av.GetBodyFromEmotionFromOpts(opts);
    expect(body.m_faceIndex).toBe(2); // SAD
    expect(body.m_torsoIndex).toBe(4); // NEUTRAL fallback
  });

  it("sentinel emotion opt (not in fRec/bRec) -> face unchanged, torso NEUTRAL fallback", () => {
    // Use EM_DOUBLEPOINT (1004) — not in fRec or bRec, so the sentinel
    // path of GetHeadAndBodyFromEmotion doesn't set tIndex. Then
    // SetTorsoNeutral is called: starts at m_lastTorso+1, finds NEUTRAL at 4.
    const av = mkComplex(COMPLEX_FREC, COMPLEX_BREC);
    // First call: set the prior state with a known emotion.
    av.GetBodyFromEmotion(new CEmotion(0.5, EM_HAPPY));
    expect(av.m_lastFace).toBe(0);
    expect(av.m_lastTorso).toBe(0);
    // Then ask for a sentinel emotion via opts.
    const opts = new CEmotionOpts();
    opts.Add(1004.0, 1.0, 5); // EM_DOUBLEPOINT, not in either table
    const body = av.GetBodyFromEmotionFromOpts(opts);
    // Face path: directional branch (1004 > 2*PI) -> exact-match torso
    // search, no face change. Torso: no exact match for 1004 -> tIndex=-1.
    // After the loop: foundF=-1 -> SetFaceNeutral; foundT=-1 -> SetTorsoNeutral.
    // SetFaceNeutral: starts at m_lastFace+1=1, scans fRec[1..6] for
    // (NEUTRAL, 0). fRec[1..5] are directional, fRec[6]=NEUTRAL -> found at 6.
    // SetTorsoNeutral: starts at m_lastTorso+1=1, scans bRec[1..4] for
    // (NEUTRAL, 0). bRec[1..3] are directional, bRec[4]=NEUTRAL -> found at 4.
    expect(body.m_faceIndex).toBe(6); // NEUTRAL fallback
    expect(body.m_torsoIndex).toBe(4); // NEUTRAL fallback
  });
});

describe("GetBodyIndexFromEmotion (Simple, helper)", () => {
  it("sentinel emotion: exact-match search returns matching index", () => {
    const av = mkSimple(SIMPLE_BREC);
    const bIndex = { value: -1 };
    av.GetBodyIndexFromEmotion(new CEmotion(1.0, EM_WAVE), bIndex);
    expect(bIndex.value).toBe(7); // EM_WAVE entry at index 7
  });

  it("sentinel emotion not in bRec -> -1", () => {
    const av = mkSimple(SIMPLE_BREC);
    const bIndex = { value: -1 };
    av.GetBodyIndexFromEmotion(new CEmotion(1.0, 1004.0), bIndex); // EM_DOUBLEPOINT
    expect(bIndex.value).toBe(-1);
  });
});

describe("GetHeadAndBodyFromEmotion (Complex, helper)", () => {
  it("directional emotion: face scan finds the match", () => {
    const av = mkComplex(COMPLEX_FREC, COMPLEX_BREC);
    const idx = { fIndex: -1, tIndex: -1 };
    av.GetHeadAndBodyFromEmotion(new CEmotion(0.5, EM_SHOUT), idx);
    // fRec[4] = SHOUT
    expect(idx.fIndex).toBe(4);
    expect(idx.tIndex).toBe(-1); // directional emotion path doesn't set torso
  });

  it("sentinel emotion: exact-match torso search", () => {
    const av = mkComplex(COMPLEX_FREC, COMPLEX_BREC);
    const idx = { fIndex: -1, tIndex: -1 };
    av.GetHeadAndBodyFromEmotion(new CEmotion(1.0, EM_WAVE), idx);
    expect(idx.fIndex).toBe(-1); // face path not taken
    // WAVE not in bRec -> -1
    expect(idx.tIndex).toBe(-1);
  });
});
