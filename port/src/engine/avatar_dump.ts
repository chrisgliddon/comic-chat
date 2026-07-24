/**
 * Avatar pose probe dumper — the TS mirror of `CaptureAvatarPose` in
 * `oracle/harness/oracleharness.cpp`. Constructs synthetic `CAvatarSimple`
 * and `CAvatarComplex` with known bRec/fRec arrays, runs a probe battery
 * of `GetBodyFromEmotion` calls, and emits the integer selection +
 * post-state m_last* so the oracle golden test can diff byte-for-byte.
 *
 * KEEP THE PROBE LIST IN SYNC with `CaptureAvatarPose` in the C++ harness.
 * Both must construct the same bRec/fRec arrays in the same order, run
 * the same emotion sequence, and dump the same fields.
 *
 * **PI precision:** the dump uses the v2.5 PI (the C oracle's value).
 * The v1.0-pre `PI = 3.14159` in `port/src/core/numeric.ts` would
 * produce different selection for sentinel-emotion inputs (the
 * PI/8 threshold flips); the `value_to_angle_v25` and
 * `subtract_angles_v25` in `avatar.ts` use v2.5 PI for that reason.
 */
import { CAvatarComplex, CAvatarSimple, type BODYREC, type FACEREC, type RBODYREC } from "./avatar.js";
import { CEmotion } from "../core/emotionopts.js";
import {
  EM_ANGRY,
  EM_HAPPY,
  EM_LAUGH,
  EM_NEUTRAL,
  EM_SAD,
  EM_SCARED,
  EM_SHOUT,
  EM_WAVE,
} from "../core/emotion.js";

// ---------------------------------------------------------------------------
// Synthetic avatar data — must match the C++ CaptureAvatarPose exactly.
// 8 directional bRec entries at intensity 0.5 (one per wheel position) +
// 1 NEUTRAL (intensity 0) + 1 WAVE.
// ---------------------------------------------------------------------------
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

// Complex avatar: faces 0..6 (HAPPY..LAUGH + NEUTRAL), torsos 0..3
// (HAPPY/SAD/ANGRY/SHOUT) + NEUTRAL at 4.
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

export interface AvatarProbeResult {
  input: { emotion: string; intensity: string };
  bodyIndex: number;
  m_lastBody: number;
}
export interface AvatarComplexProbeResult {
  input: { emotion: string; intensity: string };
  faceIndex: number;
  torsoIndex: number;
  m_lastFace: number;
  m_lastTorso: number;
}
export interface AvatarDump {
  simple: AvatarProbeResult[];
  complex: AvatarComplexProbeResult[];
}

// Probe battery — must match the C++ CaptureAvatarPose probes[] array.
// Each entry is the (emotion, intensity) pair fed into GetBodyFromEmotion.
// We deliberately include sentinel emotions and out-of-range values to
// exercise the C quirks (angle-after-normalization, neutral fallback,
// history-bias scan start).
const SIMPLE_PROBES: readonly { emotion: number; intensity: number }[] = [
  { emotion: EM_HAPPY, intensity: 0.5 },
  { emotion: EM_SAD, intensity: 0.5 },
  { emotion: EM_SHOUT, intensity: 0.5 },
  { emotion: EM_LAUGH, intensity: 0.5 },
  { emotion: EM_SCARED, intensity: 0.5 },
  { emotion: EM_ANGRY, intensity: 0.5 },
  { emotion: EM_WAVE, intensity: 1.0 }, // sentinel — angle quirk
  { emotion: 1004.0, intensity: 1.0 },  // EM_DOUBLEPOINT sentinel
  { emotion: 9999.0, intensity: 1.0 },  // unknown sentinel
];

const COMPLEX_PROBES: readonly { emotion: number; intensity: number }[] = [
  { emotion: EM_HAPPY, intensity: 0.5 },
  { emotion: EM_SAD, intensity: 0.5 },
  { emotion: EM_SHOUT, intensity: 0.5 },
  { emotion: EM_LAUGH, intensity: 0.5 },
  { emotion: EM_SCARED, intensity: 0.5 },
  { emotion: EM_ANGRY, intensity: 0.5 },
  { emotion: EM_WAVE, intensity: 1.0 },
  { emotion: 1004.0, intensity: 1.0 },
];

export function dumpAvatarProbes(): AvatarDump {
  // Simple sequence: FRESH avatar per probe so m_lastBody starts at -1.
  // (The dump must match the C++ harness, which creates a fresh
  // CAvatarSimple per probe — the C++ uses stack arrays and re-initializes
  // m_lastBody = -1 before each call.)
  const simple: AvatarProbeResult[] = [];
  for (const probe of SIMPLE_PROBES) {
    const av = new CAvatarSimple();
    av.bRec = SIMPLE_BREC;
    av.m_nBodies = SIMPLE_BREC.length;
    av.m_lastBody = -1;
    const body = av.GetBodyFromEmotion(new CEmotion(probe.intensity, probe.emotion));
    simple.push({
      input: {
        emotion: String(probe.emotion),
        intensity: String(probe.intensity),
      },
      bodyIndex: body.m_bodyIndex,
      m_lastBody: av.m_lastBody,
    });
  }

  const complex: AvatarComplexProbeResult[] = [];
  for (const probe of COMPLEX_PROBES) {
    const av = new CAvatarComplex();
    av.fRec = COMPLEX_FREC;
    av.bRec = COMPLEX_BREC;
    av.nFaces = COMPLEX_FREC.length;
    av.nTorsos = COMPLEX_BREC.length;
    av.m_lastFace = -1;
    av.m_lastTorso = -1;
    const body = av.GetBodyFromEmotion(new CEmotion(probe.intensity, probe.emotion));
    complex.push({
      input: {
        emotion: String(probe.emotion),
        intensity: String(probe.intensity),
      },
      faceIndex: body.m_faceIndex,
      torsoIndex: body.m_torsoIndex,
      m_lastFace: av.m_lastFace,
      m_lastTorso: av.m_lastTorso,
    });
  }

  return { simple, complex };
}
