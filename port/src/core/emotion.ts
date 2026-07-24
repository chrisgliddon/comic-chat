/**
 * Emotion constants and the emotion-wheel quantization.
 *
 * Source: `v2.5-beta-1-modern/avatar.h:254-267` (the golden oracle tree).
 * The 8 "directional" emotions live on a wheel at multiples of `2*PI/8`;
 * three special non-directional emotions (WAVE, POINTOTHER, POINTSELF) use
 * sentinel values >1000.
 *
 * **PI is tree-dependent (RULEBOOK §geometry nuance):** v1.0-pre-modern
 * defines `PI = 3.14159` (vector2d.h:39) but v2.5-beta-1-modern defines
 * `PI = 3.14159265358979323846` (vector2d.h:54). The emotion constants come
 * from the v2.5 oracle tree, so they use the full-precision PI. This was
 * caught by the oracle textpose golden: the float32-cast emotion values
 * (4.7123889923095703 for SHOUT, etc.) only match when PI is the full-
 * precision value. The spline/bbox/vector2d port used the v1.0-pre PI for
 * its angle routines and got 67/67 bezpt match — because the spline code
 * never touches PI. The two PI values coexist: angle routines use the
 * vector2d.h literal of whichever tree; emotion constants use the v2.5 PI.
 *
 * **Float cast:** the C defines cast to `(float)` — truncating double→float32.
 * JS numbers are doubles, so we replicate with `Math.fround`. The oracle emits
 * these as "%.17g" strings of the float32 bit pattern (e.g. 4.7123889923095703);
 * the port must fround to match.
 */
// The v2.5 oracle tree's PI (vector2d.h:54). NOT the v1.0-pre 3.14159.
const PI_V25 = 3.14159265358979323846;
const TWO_PI_V25 = 2 * PI_V25;

// The 8 directional emotions. C: `((float)(k * 2 * PI / 8))`. Computed in
// double, then cast to float32 via Math.fround. Keep the exact C evaluation
// order `(k * 2 * PI / 8)` so the pre-cast double matches.
export const EM_HAPPY = Math.fround((0 * 2 * PI_V25) / 8); // 0
export const EM_COY = Math.fround((1 * 2 * PI_V25) / 8);
export const EM_BORED = Math.fround((2 * 2 * PI_V25) / 8);
export const EM_SCARED = Math.fround((3 * 2 * PI_V25) / 8);
export const EM_SAD = Math.fround((4 * 2 * PI_V25) / 8);
export const EM_ANGRY = Math.fround((5 * 2 * PI_V25) / 8);
export const EM_SHOUT = Math.fround((6 * 2 * PI_V25) / 8);
export const EM_LAUGH = Math.fround((7 * 2 * PI_V25) / 8);

// NEUTRAL is the special "no emotion" value. C: `((float)0.0)` (avatar.h:264).
// The avatario `emFloats[9]` entry is EM_NEUTRAL — distinct from emFloats[0]
// which is also 0.0; the table is built that way for index-0 padding reasons.
// Both are bit-identical to 0.0 so the encoder treats them as the same.
export const EM_NEUTRAL = 0.0;

// Special (non-directional) emotions — sentinel floats. These are integer-
// valued so no float-cast precision issue. All eight are used in the avatario
// `emFloats[]` table (avatario.cpp:45-64) and in `GetBodyFromEmotion`'s
// nearest-neighbor selection.
export const EM_WAVE = 1001.0;
export const EM_POINTOTHER = 1002.0;
export const EM_POINTSELF = 1003.0;
export const EM_DOUBLEPOINT = 1004.0;
export const EM_SHRUG = 1005.0;
export const EM_3QRWALK = 1006.0;
export const EM_SIDEWALK = 1007.0;
export const EM_3QFWALK = 1008.0;

// CEmotionOpts limits (avatar.h:23-25)
export const MAXEMOPTS = 10;
export const OVERRIDEBYPRIORITY = 1;
export const ADDPRIORITY = 2;