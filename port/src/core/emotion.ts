/**
 * Emotion constants and the emotion-wheel quantization.
 *
 * Source: `v1.0-pre-modern/avatar.h:254-267` (byte-identical to the v2.5
 * oracle tree). The 8 "directional" emotions live on a wheel at multiples of
 * `2*PI/8`; three special non-directional emotions (WAVE, POINTOTHER,
 * POINTSELF) use sentinel values >1000.
 *
 * PI is the low-precision literal 3.14159 (RULEBOOK §geometry; vector2d.h:39).
 * Do NOT substitute Math.PI — that would shift every emotion angle and break
 * the `EmotionToBytes`/`BytesToEmotion` round-trip (Tier-1 #3, to be ported
 * with avatario.cpp).
 */
import { PI } from "./numeric.js";

export const TWO_PI = 2 * PI;

// The 8 directional emotions. Computed as `k * 2 * PI / 8` for k=0..7.
// Keep the exact C evaluation order `(k * 2 * PI / 8)` so float results match.
export const EM_HAPPY = (0 * 2 * PI) / 8; // 0
export const EM_COY = (1 * 2 * PI) / 8;
export const EM_BORED = (2 * 2 * PI) / 8;
export const EM_SCARED = (3 * 2 * PI) / 8;
export const EM_SAD = (4 * 2 * PI) / 8;
export const EM_ANGRY = (5 * 2 * PI) / 8;
export const EM_SHOUT = (6 * 2 * PI) / 8;
export const EM_LAUGH = (7 * 2 * PI) / 8;

// Special (non-directional) emotions — sentinel floats.
export const EM_WAVE = 1001.0;
export const EM_POINTOTHER = 1002.0;
export const EM_POINTSELF = 1003.0;

// CEmotionOpts limits (avatar.h:23-25)
export const MAXEMOPTS = 10;
export const OVERRIDEBYPRIORITY = 1;
export const ADDPRIORITY = 2;