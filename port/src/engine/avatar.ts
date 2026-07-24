/**
 * Port of `avatar.cpp` — emotion-to-pose selection (Tier-1 #2 in plan doc).
 *
 * Source: `v2.5-beta-1-modern/avatar.cpp` (the golden oracle tree; the
 * v1.0-pre-modern version differs only in pose loading/deletion code that
 * is irrelevant to the GetBodyFromEmotion logic).
 *
 * What this file ports:
 *   - The pose-record structs `RBODYREC` / `FACEREC` / `BODYREC` (avatar.h:103-129).
 *   - The body-class hierarchy: `CBody` base, `CBodySingle` (one RBODYREC),
 *     `CBodyDouble` (FACEREC + BODYREC) — only the fields and methods used
 *     by the emotion-to-pose selection path. Rendering (`Draw`, `DrawBody`)
 *     and pose-image access (`GetDimInfo`) are NOT ported here — they
 *     depend on `CPose`/`CAvatarDIB` which is Phase 4 `avbfile`/`dib` work.
 *   - `CAvatarSimple::GetBodyFromEmotion(CEmotion &)` — the single-emotion
 *     nearest-neighbor over `bRec[]`. Skips emotions > 7 (the directional
 *     wheel is indices 0..7; values > 7 are the sentinel emotions).
 *   - `CAvatarComplex::GetBodyFromEmotion(CEmotion &)` — face scan over
 *     `fRec[]` (no threshold) + torso scan over `bRec[]` (with PI/NEMOTIONS
 *     threshold, starts at m_lastTorso+1).
 *   - `GetBodyIndexFromEmotion` / `GetHeadAndBodyFromEmotion` — the helpers
 *     used by the CEmotionOpts entry point.
 *   - `SetBodyNeutral` / `SetFaceNeutral` / `SetTorsoNeutral` — the fallback
 *     (find a `(emotion=NEUTRAL, intensity=0)` entry starting from last*+1).
 *   - The CEmotionOpts variants of `GetBodyFromEmotion` — the actual entry
 *     point used by the engine (consumes a `CEmotionOpts` from textpose).
 *
 * **PI is tree-dependent (RULEBOOK §1.2):** `value_to_angle` in
 * `port/src/engine/vector2d.ts` uses the v1.0-pre `PI = 3.14159` because the
 * Phase-3 spline golden was pinned on that value (spline never touches PI).
 * The v2.5 oracle compiles with `PI = 3.14159265358979323846`. For the
 * GetBodyFromEmotion selection, this is **benign**: the `PI/NEMOTIONS`
 * threshold is `~0.3927`, the smallest non-zero emotion angle is
 * `2*PI/8 = PI/4 ≈ 0.7854`, so only EXACT-MATCH candidates (angle=0) pass
 * the threshold — the actual integer index picked is the same regardless of
 * PI precision. The `nearestAngle` float DOES differ between port and
 * oracle (5th-6th decimal); the dump therefore only emits integer indices
 * + post-state, not the angle values.
 *
 * **The m_lastBody/Face/Torso history state matters** (plan doc §5 Tier-1
 * #2: "Run sequences, not single messages"). The `index = (m_last* + 1 + i)
 * % m_n*` wraparound in the torso/body scan means the chosen entry
 * influences the NEXT call's start position. The dump probe battery drives
 * a sequence of emotion calls and captures the m_last* state at each step
 * so the test catches divergence in the history state.
 *
 * **The dump is synthetic-avatar driven.** The real engine path goes
 * through the `.avb` file loader (Phase 4 `avbfile.cpp` work — not yet
 * ported). The Tier-1 #2 oracle dump constructs a synthetic `CAvatarSimple`
 * with a known `bRec[]` (and similarly `CAvatarComplex` with known
 * `fRec[]`/`bRec[]`), runs a probe battery of `GetBodyFromEmotion` calls,
 * and emits the integer indices + m_last* state. The TS port matches
 * against this on the same synthetic data.
 */
import { CEmotion } from "../core/emotionopts.js";
import { CEmotionOpts } from "../core/emotionopts.js";
import {
  EM_NEUTRAL,
  EM_WAVE,
} from "../core/emotion.js";

// ---------------------------------------------------------------------------
// **PI precision for the angle metric (RULEBOOK §1.2 + §14 new):** the
// port's `vector2d.subtract_angles` uses v1.0-pre `PI = 3.14159` (pinned
// by the Phase-3 spline golden, which never touches PI). The C oracle
// compiles with v2.5's full-precision `PI = 3.14159265358979323846`. For
// the `value_to_angle` / `subtract_angles` calls in this file, we use
// the v2.5 PI (declared below as PI_V25 / TWO_PI_V25). This is REQUIRED
// for the threshold check `thisAngle < PI/8` to reproduce the C
// behavior: the threshold is `~0.39269908` (v2.5) vs `0.3927` (v1.0-pre),
// and the difference flips the selection for some sentinel-emotion inputs
// (e.g. EM_WAVE=1001 → SCARED/PI/8 boundary). The avatario/avatar dump
// is pinned to the v2.5 forms; the spline golden is pinned to the
// v1.0-pre forms.
// ---------------------------------------------------------------------------
const PI_V25 = 3.14159265358979323846;
const TWO_PI_V25 = 2 * PI_V25;

// v2.5 value_to_angle — uses the FULL-PRECISION PI.
// Identical to vector2d.value_to_angle except for the PI source.
function value_to_angle_v25(value: number): number {
  if (value > -PI_V25 && value <= PI_V25) return value;
  let temp = value / TWO_PI_V25;
  temp = (temp - Math.trunc(temp)) * 2 * PI_V25;
  if (temp > PI_V25) return temp - TWO_PI_V25;
  else if (temp <= -PI_V25) return temp + TWO_PI_V25;
  else return temp;
}

// v2.5 subtract_angles — used by the GetBodyFromEmotion angle metric.
function subtract_angles_v25(angle1: number, angle2: number): number {
  return value_to_angle_v25(angle1 - angle2);
}

// ---------------------------------------------------------------------------
// Constants from avatar.h:254-348
// ---------------------------------------------------------------------------
// NEMOTIONS = 8 (the 8 directional emotions; the wheel has 8 slots).
// The threshold `PI / NEMOTIONS` filters candidates in the torso/body scan.
export const NEMOTIONS = 8;
// 2*PI sentinel threshold: emotions with m_emotion > 2*PI (the special
// sentinels WAVE=1001, POINTOTHER=1002, ...) cannot use the angle-based
// metric. The C code branches on this in GetBodyIndexFromEmotion and
// GetHeadAndBodyFromEmotion.
export const EMOTION_ANGLE_MAX = 2 * PI_V25;
// 7 = max index of directional emotions (EM_HAPPY=0..EM_LAUGH=7). Sentinels
// start at 1001. The C torso/body scan `if (bRec[index].emotion > 7) continue;`
// skips non-directional entries (the "out of wheel" filter).
export const DIRECTIONAL_MAX_INDEX = 7;

// ---------------------------------------------------------------------------
// Record types (avatar.h:103-129)
// ---------------------------------------------------------------------------
// RBODYREC (avatar.h:123-129) — single-pose avatar body record.
export interface RBODYREC {
  poseID: number;
  emotion: number;
  intensity: number;
  faceX: number;
  faceY: number;
}

// FACEREC (avatar.h:103-113) — complex avatar face record (head).
export interface FACEREC {
  poseID: number;
  emotion: number;
  intensity: number;
  xCX: number;
  yCX: number;
  delta_xCX: number;
  delta_yCX: number;
  faceX: number;
  faceY: number;
}

// BODYREC (avatar.h:115-121) — complex avatar torso record.
export interface BODYREC {
  poseID: number;
  emotion: number;
  intensity: number;
  xCX: number;
  yCX: number;
}

// ---------------------------------------------------------------------------
// CBody hierarchy (avatar.h:83-167)
// ---------------------------------------------------------------------------
// CBody base. We only port the fields touched by GetBodyFromEmotion:
// m_avatarID, m_flip, m_requested. The CPanelElement base (which contributes
// m_bbox) is NOT ported — it would require panel.cpp work that comes later.
// The port models the same field set the C has at the GetBodyFromEmotion
// call site.
export class CBody {
  m_avatarID: number;
  m_flip: number;
  m_requested: number;
  m_arrowX: number;

  constructor(avID = 0) {
    this.m_avatarID = avID;
    this.m_flip = 0;
    this.m_requested = 1; // C: TRUE
    this.m_arrowX = 0;
  }
}

// CBodySingle — uses RBODYREC. The C stores a pointer into the avatar's
// bRec[] array; the port stores the array INDEX (or null for "not set").
// The index is sufficient to identify the pose; the actual record lookup
// happens at the consumer (which Phase-4 avbfile.cpp provides).
export class CBodySingle extends CBody {
  m_bodyIndex: number; // -1 if not set; otherwise the index into bRec[]
  constructor(avID = 0) {
    super(avID);
    this.m_bodyIndex = -1;
  }
}

// CBodyDouble — uses FACEREC (face) + BODYREC (torso).
export class CBodyDouble extends CBody {
  m_faceIndex: number;
  m_torsoIndex: number;
  constructor(avID = 0) {
    super(avID);
    this.m_faceIndex = -1;
    this.m_torsoIndex = -1;
  }
}

// ---------------------------------------------------------------------------
// CAvatarSimple — owns bRec[] of RBODYREC, nBodies, lastBody.
// avatar.h:259-284, avatar.cpp:226-253, 330-356, 390-416, 445-456.
// ---------------------------------------------------------------------------
export class CAvatarSimple {
  bRec: RBODYREC[] = [];
  m_nBodies: number = 0;
  m_lastBody: number = -1;

  // avatar.cpp:226-253. Single-emotion nearest-neighbor over bRec[].
  // Skips entries with emotion > 7 (sentinel emotions; they don't fit the
  // angle metric and would always lose to a directional match anyway).
  // The neutral fallback: if no entry passes the PI/NEMOTIONS threshold,
  // the FIRST entry with (emotion==NEUTRAL, intensity==0) wins.
  GetBodyFromEmotion(emotion: CEmotion): CBodySingle {
    const body = new CBodySingle();
    let nearestAngle = 3 * PI_V25;
    let intensityOfNearest = 2.0;
    let nearestI = -1;

    for (let i = 0; i < this.m_nBodies; i++) {
      // C: `index = (m_lastBody + 1 + i) % m_nBodies` — start search at
      // m_lastBody+1, wrap around. This is the m_lastBody history bias.
      const index = (this.m_lastBody + 1 + i) % this.m_nBodies;
      if (this.bRec[index].emotion > DIRECTIONAL_MAX_INDEX) continue;
      const thisAngle = Math.abs(
        subtract_angles_v25(this.bRec[index].emotion, emotion.m_emotion),
      );
      // C: `BOOL isFirstNeutral = bRec[index].emotion == EM_NEUTRAL && bRec[index].intensity == 0.0 && nearestI == -1`.
      // Considered the first neutral (only if we haven't picked anything yet)
      // — it's the fallback if no directional match passes the threshold.
      const isFirstNeutral =
        this.bRec[index].emotion === EM_NEUTRAL &&
        this.bRec[index].intensity === 0.0 &&
        nearestI === -1;
      if (thisAngle < PI_V25 / NEMOTIONS || isFirstNeutral) {
        // C: `if (isFirstNeutral && emotion.m_intensity > 0.0) delta_i = 1.5;`
        // — neutral fallback is "less powerful than any correct match" so it
        // only wins if there's no real match.
        let delta_i: number;
        if (isFirstNeutral && emotion.m_intensity > 0.0) delta_i = 1.5;
        else delta_i = Math.abs(emotion.m_intensity - this.bRec[index].intensity);
        if (delta_i < intensityOfNearest) {
          nearestAngle = thisAngle;
          intensityOfNearest = delta_i;
          nearestI = index;
        }
      }
    }

    if (nearestI >= 0) {
      body.m_bodyIndex = nearestI;
    } else {
      // No match — fall back to neutral body. C calls SetBodyNeutral which
      // starts the search at m_lastBody+1 looking for (NEUTRAL, 0). The
      // final fallback is index 0 ("Oh well, just set it to first").
      this.SetBodyNeutral(body);
    }
    this.m_lastBody = body.m_bodyIndex;
    return body;
  }

  // avatar.cpp:330-356. Helper used by the CEmotionOpts variant.
  // Sentinel emotions (m_emotion > 2*PI) do an exact-match search instead
  // of the angle-based metric. Returns bIndex via the out-param; -1 if no
  // match. The dump observes the chosen index and the post-state.
  GetBodyIndexFromEmotion(emotion: CEmotion, bIndex: { value: number }): void {
    bIndex.value = -1;
    if (emotion.m_emotion <= EMOTION_ANGLE_MAX) {
      let nearestAngle = 3 * PI_V25;
      let intensityOfNearest = 2.0;
      for (let i = 0; i < this.m_nBodies; i++) {
        const thisAngle = Math.abs(
          subtract_angles_v25(this.bRec[i].emotion, emotion.m_emotion),
        );
        if (thisAngle <= nearestAngle) {
          const delta_i = Math.abs(
            emotion.m_intensity - this.bRec[i].intensity,
          );
          // Tiebreak: if equal angle, prefer the one with SMALLER intensity
          // difference (the `<` here; the `continue` is the opposite).
          if (thisAngle === nearestAngle && delta_i >= intensityOfNearest) continue;
          nearestAngle = thisAngle;
          intensityOfNearest = delta_i;
          bIndex.value = i;
        }
      }
    } else {
      // Sentinel emotion: exact-match search.
      for (let i = 0; i < this.m_nBodies; i++) {
        if (emotion.m_emotion === this.bRec[i].emotion) {
          bIndex.value = i;
          break;
        }
      }
    }
  }

  // avatar.cpp:390-416. CEmotionOpts variant — the actual entry point.
  // Iterates opts by descending priority, picks the first with a match.
  // Falls back to SetBodyNeutral if no opt matched.
  GetBodyFromEmotionFromOpts(opts: CEmotionOpts): CBodySingle {
    const body = new CBodySingle();
    let foundB = -1;
    while (true) {
      let bestIndex = -1;
      let minPriority = 0;
      for (let i = 0; i < opts.m_nOpts; i++) {
        if (opts.m_priorities[i] > minPriority) {
          bestIndex = i;
          minPriority = opts.m_priorities[i];
        }
      }
      if (!minPriority) break;
      const bIndex = { value: -1 };
      this.GetBodyIndexFromEmotion(opts.m_emotions[bestIndex], bIndex);
      opts.m_priorities[bestIndex] = 0; // nuke so we don't kill again
      if (bIndex.value >= 0 && foundB < 0) {
        body.m_bodyIndex = bIndex.value;
        foundB = bIndex.value;
        break;
      }
    }
    if (foundB < 0) this.SetBodyNeutral(body);
    this.m_lastBody = body.m_bodyIndex;
    return body;
  }

  // avatar.cpp:445-456. Fallback: search (NEUTRAL, 0) starting at
  // m_lastBody+1; final fallback to index 0.
  SetBodyNeutral(body: CBodySingle): void {
    let c = this.m_lastBody;
    for (let i = 0; i < this.m_nBodies; i++) {
      c = (c + 1) % this.m_nBodies;
      if (this.bRec[c].emotion === EM_NEUTRAL && this.bRec[c].intensity === 0.0) {
        body.m_bodyIndex = c;
        return;
      }
    }
    body.m_bodyIndex = 0; // oh well
  }
}

// ---------------------------------------------------------------------------
// CAvatarComplex — owns fRec[] (faces) + bRec[] (torsos), nFaces/nTorsos,
// lastFace/lastTorso. avatar.h:290-324, avatar.cpp:256-300, 302-328, 358-388.
// ---------------------------------------------------------------------------
export class CAvatarComplex {
  fRec: FACEREC[] = [];
  bRec: BODYREC[] = [];
  nFaces: number = 0;
  nTorsos: number = 0;
  m_lastFace: number = -1;
  m_lastTorso: number = -1;

  // avatar.cpp:256-300. Single-emotion face+torso selection.
  // Face: scan all fRec[] with no threshold (smallest angle wins; intensity
  // tiebreak). Torso: start at m_lastTorso+1, skip emotion>7, threshold
  // is PI/NEMOTIONS + neutral-acceptance. Face scan and torso scan have
  // DIFFERENT selection rules — this is the "Distance metric needs
  // rethinking!" comment in the C source.
  GetBodyFromEmotion(emotion: CEmotion): CBodyDouble {
    const body = new CBodyDouble();

    // Face scan — no threshold, just closest angle.
    let nearestFaceAngle = 3 * PI_V25;
    let intensityOfNearest = 2.0;
    let nearestI = -1;
    for (let i = 0; i < this.nFaces; i++) {
      const thisAngle = Math.abs(
        subtract_angles_v25(this.fRec[i].emotion, emotion.m_emotion),
      );
      if (thisAngle <= nearestFaceAngle) {
        const delta_i = Math.abs(emotion.m_intensity - this.fRec[i].intensity);
        if (thisAngle === nearestFaceAngle && delta_i >= intensityOfNearest) continue;
        nearestFaceAngle = thisAngle;
        intensityOfNearest = delta_i;
        nearestI = i;
      }
    }
    if (nearestI >= 0) body.m_faceIndex = nearestI;
    else this.SetFaceNeutral(body);

    // Torso scan — threshold + history bias.
    intensityOfNearest = 2.0;
    nearestI = -1;
    for (let i = 0; i < this.nTorsos; i++) {
      const index = (this.m_lastTorso + 1 + i) % this.nTorsos;
      if (this.bRec[index].emotion > DIRECTIONAL_MAX_INDEX) continue;
      const thisAngle = Math.abs(
        subtract_angles_v25(this.bRec[index].emotion, emotion.m_emotion),
      );
      if (
        thisAngle < PI_V25 / NEMOTIONS ||
        (this.bRec[index].emotion === EM_NEUTRAL && this.bRec[index].intensity === 0)
      ) {
        const delta_i = Math.abs(
          emotion.m_intensity - this.bRec[index].intensity,
        );
        if (delta_i < intensityOfNearest) {
          intensityOfNearest = delta_i;
          nearestI = index;
        }
      }
    }
    if (nearestI >= 0) body.m_torsoIndex = nearestI;
    else this.SetTorsoNeutral(body);

    this.m_lastFace = body.m_faceIndex;
    this.m_lastTorso = body.m_torsoIndex;
    return body;
  }

  // avatar.cpp:302-328. Helper for the CEmotionOpts variant.
  // For emotion.m_emotion <= 2*PI: face scan with no threshold (above).
  // For sentinel emotions: exact-match torso search, no face change.
  GetHeadAndBodyFromEmotion(
    emotion: CEmotion,
    indices: { fIndex: number; tIndex: number },
  ): void {
    indices.fIndex = -1;
    indices.tIndex = -1;
    if (emotion.m_emotion <= EMOTION_ANGLE_MAX) {
      let nearestFaceAngle = 3 * PI_V25;
      let intensityOfNearest = 2.0;
      for (let i = 0; i < this.nFaces; i++) {
        const thisAngle = Math.abs(
          subtract_angles_v25(this.fRec[i].emotion, emotion.m_emotion),
        );
        if (thisAngle <= nearestFaceAngle) {
          const delta_i = Math.abs(
            emotion.m_intensity - this.fRec[i].intensity,
          );
          if (thisAngle === nearestFaceAngle && delta_i >= intensityOfNearest) continue;
          nearestFaceAngle = thisAngle;
          intensityOfNearest = delta_i;
          indices.fIndex = i;
        }
      }
    } else {
      // Sentinel: exact-match torso search, leave face index alone.
      for (let i = 0; i < this.nTorsos; i++) {
        if (emotion.m_emotion === this.bRec[i].emotion) {
          indices.tIndex = i;
          break;
        }
      }
    }
  }

  // avatar.cpp:358-388. CEmotionOpts variant — the actual entry point.
  // Iterates opts by descending priority; first opt that yields both face
  // AND torso is used. Falls back to SetFaceNeutral/SetTorsoNeutral.
  GetBodyFromEmotionFromOpts(opts: CEmotionOpts): CBodyDouble {
    const body = new CBodyDouble();
    let foundF = -1;
    let foundT = -1;
    while (true) {
      let bestIndex = -1;
      let minPriority = 0;
      for (let i = 0; i < opts.m_nOpts; i++) {
        if (opts.m_priorities[i] > minPriority) {
          bestIndex = i;
          minPriority = opts.m_priorities[i];
        }
      }
      if (!minPriority) break;
      const indices = { fIndex: -1, tIndex: -1 };
      this.GetHeadAndBodyFromEmotion(opts.m_emotions[bestIndex], indices);
      opts.m_priorities[bestIndex] = 0;
      if (indices.fIndex >= 0 && foundF < 0) {
        body.m_faceIndex = indices.fIndex;
        foundF = indices.fIndex;
      }
      if (indices.tIndex >= 0 && foundT < 0) {
        body.m_torsoIndex = indices.tIndex;
        foundT = indices.tIndex;
      }
      if (foundF >= 0 && foundT >= 0) break;
    }
    if (foundF < 0) this.SetFaceNeutral(body);
    if (foundT < 0) this.SetTorsoNeutral(body);
    this.m_lastFace = body.m_faceIndex;
    this.m_lastTorso = body.m_torsoIndex;
    return body;
  }

  // avatar.cpp:419-430.
  SetTorsoNeutral(body: CBodyDouble): void {
    let c = this.m_lastTorso;
    for (let i = 0; i < this.nTorsos; i++) {
      c = (c + 1) % this.nTorsos;
      if (this.bRec[c].emotion === EM_NEUTRAL && this.bRec[c].intensity === 0.0) {
        body.m_torsoIndex = c;
        return;
      }
    }
    body.m_torsoIndex = 0;
  }

  // avatar.cpp:432-443.
  SetFaceNeutral(body: CBodyDouble): void {
    let c = this.m_lastFace;
    for (let i = 0; i < this.nFaces; i++) {
      c = (c + 1) % this.nFaces;
      if (this.fRec[c].emotion === EM_NEUTRAL && this.fRec[c].intensity === 0.0) {
        body.m_faceIndex = c;
        return;
      }
    }
    body.m_faceIndex = 0;
  }
}
