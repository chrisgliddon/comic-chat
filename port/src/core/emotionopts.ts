/**
 * CEmotion + CEmotionOpts — the emotion-option accumulator.
 *
 * Source: `v1.0-pre-modern/avatar.h:14-36` + `avatar.cpp:714-737`
 * (byte-identical to the v2.5 oracle tree). `CEmotionOpts` collects the
 * emotions detected by `GetEmotionsFromString` (textpose.ts), deduplicating
 * by emotion value with a priority-resolve rule.
 *
 * RULEBOOK §Value-Types: plain TS classes, no MFC inheritance. The fixed-size
 * `m_emotions[MAXEMOPTS]` array maps to a JS array capped at MAXEMOPTS; the C
 * `UCHAR` priority fields are stored as numbers (0-255 range, see §Integer-
 * width: do not enforce the 8-bit wrap; an overflow flags a port bug).
 */
import {
  ADDPRIORITY,
  MAXEMOPTS,
  OVERRIDEBYPRIORITY,
} from "./emotion.js";

// CEmotion (avatar.h:14-21). m_intensity and m_emotion are `float` in C —
// stored as JS numbers. The C ctor casts via `(float)`, which truncates
// double→float precision; JS numbers are doubles. This matters for the
// emotion-wheel values (they're already float-precision in the constants),
// and for intensities passed in as doubles (e.g. 1.0 — exact). Pin float
// equivalence when avatario's EmotionToBytes is ported (it reinterprets the
// float bit pattern); for textpose, the intensities are all exact in double.
export class CEmotion {
  m_intensity: number;
  m_emotion: number;
  constructor(intensity = 0, emotion = 0) {
    this.m_intensity = intensity;
    this.m_emotion = emotion;
  }
  Set(intensity: number, emotion: number): void {
    this.m_intensity = intensity;
    this.m_emotion = emotion;
  }
}

// CEmotionOpts (avatar.h:27-36, avatar.cpp:714-737)
// m_nOpts is UCHAR (0-255); m_emotions and m_priorities are MAXEMOPTS-long.
export class CEmotionOpts {
  m_nOpts: number = 0;
  m_emotions: CEmotion[] = [];
  m_priorities: number[] = [];

  constructor() {
    this.m_nOpts = 0;
    // Pre-allocate MAXEMOPTS slots to mirror the C fixed array (the C indexes
    // m_emotions[m_nOpts] directly; we do the same via push but keep the cap).
    for (let i = 0; i < MAXEMOPTS; i++) {
      this.m_emotions.push(new CEmotion());
      this.m_priorities.push(0);
    }
  }

  // avatar.cpp:714 — the two-overload Add. The int overload delegates to the
  // double overload via `Add((double) emotion, ...)`. We expose one method
  // taking a number (JS numbers unify int/double); callers pass the emotion
  // constant which is already a number.
  Add(
    emotion: number,
    intensity: number,
    priority: number,
    flags = OVERRIDEBYPRIORITY,
  ): void {
    for (let i = 0; i < this.m_nOpts; i++) {
      if (this.m_emotions[i].m_emotion === emotion) {
        if (flags & OVERRIDEBYPRIORITY) {
          if (this.m_priorities[i] < priority) {
            this.m_priorities[i] = priority;
            this.m_emotions[i].m_intensity = intensity;
          }
          return;
        } else if (flags & ADDPRIORITY) {
          // C: `max(m_priorities[i] + priority, 255)` — clamps to 255 (UCHAR max).
          this.m_priorities[i] = Math.max(this.m_priorities[i] + priority, 255);
          this.m_emotions[i].m_intensity = Math.max(
            this.m_emotions[i].m_intensity,
            intensity,
          );
          return;
        }
      }
    }

    if (this.m_nOpts >= MAXEMOPTS) return; // cap, silent drop (C behavior)

    this.m_emotions[this.m_nOpts].m_emotion = emotion;
    this.m_emotions[this.m_nOpts].m_intensity = intensity;
    this.m_priorities[this.m_nOpts] = priority;
    this.m_nOpts++;
  }
}