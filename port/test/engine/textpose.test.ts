/**
 * Golden tests for the textpose port — Tier-1 #1.
 *
 * The oracle corpus doesn't yet have a direct `GetEmotionsFromString` dump
 * mode (LEDGER TODO), so these goldens are HAND-COMPUTED from the frozen v2.5
 * rule table (chat.rc:2290-2304). This is the two-agent check: the expected
 * values below are derived by reasoning about the rules independently of the
 * port implementation; the port must reproduce them. Each test cites the rule
 * it exercises.
 *
 * Rule table summary (v2.5):
 *   SHOUT:  AllCaps("")        ;9   + FindString("!!!");9
 *   LAUGH:  CheckWord*("ROTFL");11 + CheckWord*("LOL");11 + FindString*("HEHE");11
 *   HAPPY:  FindString(":)")   ;10 + FindString(":-)");10
 *   SAD:    FindString(":(")   ;10 + FindString(":-(");10
 *   POINTOTHER: CheckStart*("You");4 + CheckWord*("are you");8 ...
 *   POINTSELF:  CheckStart*("I");3  + CheckWord*("i'm");7 ...
 *   WAVE:   CheckStart*("Hi");2 + CheckStart*("Bye");3 + CheckStart*("Hello");5 ...
 *   COY:    FindString(";-)");10 + FindString(";)");10
 *   ANGRY/SCARED/BORED: (empty — no rules)
 *
 * CEmotionOpts.Add dedup: OVERRIDEBYPRIORITY (default) replaces an existing
 * emotion's {intensity,priority} only if the new priority is HIGHER.
 */
import { describe, it, expect, beforeEach } from "vitest";
import {
  CheckForUppers,
  CheckWord,
  GetEmotionsFromString,
  InitializeEmotionRules,
} from "../../src/engine/textpose.js";
import { CEmotionOpts } from "../../src/core/emotionopts.js";
import {
  EM_COY,
  EM_HAPPY,
  EM_LAUGH,
  EM_POINTOTHER,
  EM_POINTSELF,
  EM_SAD,
  EM_SHOUT,
  EM_WAVE,
} from "../../src/core/emotion.js";

let opts: CEmotionOpts;
beforeEach(() => {
  InitializeEmotionRules();
  opts = new CEmotionOpts();
});

// Helper: extract the {emotion, intensity, priority} entries for inspection.
function entries(o: CEmotionOpts) {
  const out: { emotion: number; intensity: number; priority: number }[] = [];
  for (let i = 0; i < o.m_nOpts; i++) {
    out.push({
      emotion: o.m_emotions[i].m_emotion,
      intensity: o.m_emotions[i].m_intensity,
      priority: o.m_priorities[i],
    });
  }
  return out;
}
function findEmotion(o: CEmotionOpts, emotion: number) {
  for (let i = 0; i < o.m_nOpts; i++) if (o.m_emotions[i].m_emotion === emotion) return o.m_emotions[i];
  return null;
}

describe("CheckForUppers (AllCaps detection)", () => {
  it("true when >1 upper char and no lower", () => {
    expect(CheckForUppers("HELLO")).toBe(true);
    expect(CheckForUppers("HI")).toBe(true);
  });
  it("false when any lowercase present", () => {
    expect(CheckForUppers("Hello")).toBe(false);
    expect(CheckForUppers("HELLO world")).toBe(false);
  });
  it("false when only one upper char", () => {
    expect(CheckForUppers("A")).toBe(false);
    expect(CheckForUppers("A.")).toBe(false);
  });
  it("true for uppercase + punctuation (no lowercase)", () => {
    expect(CheckForUppers("YAY!")).toBe(true);
  });
  it("empty string -> 0 uppers -> false", () => {
    expect(CheckForUppers("")).toBe(false);
  });
});

describe("CheckWord (whole-word substring match)", () => {
  it("matches a standalone word", () => {
    expect(CheckWord("that is ROTFL", "ROTFL")).toBe(true);
  });
  it("matches at start of string", () => {
    expect(CheckWord("LOL that's funny", "LOL")).toBe(true);
  });
  it("matches at end of string", () => {
    expect(CheckWord("that's funny LOL", "LOL")).toBe(true);
  });
  it("does not match mid-word (no whitespace boundary before)", () => {
    expect(CheckWord("xLOL", "LOL")).toBe(false);
  });
  it("does not match mid-word (no boundary after)", () => {
    expect(CheckWord("LOLx", "LOL")).toBe(false);
  });
  it("punct after counts as a boundary", () => {
    expect(CheckWord("LOL!", "LOL")).toBe(true);
    expect(CheckWord("LOL.", "LOL")).toBe(true);
  });
});

describe("GetEmotionsFromString — SHOUT rules", () => {
  it("AllCaps triggers EM_SHOUT priority 9", () => {
    // "HELLO" has no lowercase and >1 upper -> AllCaps fires.
    GetEmotionsFromString("HELLO", opts);
    const e = findEmotion(opts, EM_SHOUT);
    expect(e).not.toBeNull();
    expect(e!.m_intensity).toBe(1.0);
    expect(opts.m_priorities[0]).toBe(9);
  });
  it("'!!!' triggers EM_SHOUT via FindString priority 9", () => {
    GetEmotionsFromString("what!!!", opts);
    const e = findEmotion(opts, EM_SHOUT);
    expect(e).not.toBeNull();
    expect(e!.m_intensity).toBe(1.0);
  });
  it("mixed-case 'hello' does NOT trigger SHOUT", () => {
    GetEmotionsFromString("hello", opts);
    expect(findEmotion(opts, EM_SHOUT)).toBeNull();
  });
});

describe("GetEmotionsFromString — LAUGH rules (case-insensitive)", () => {
  it("'LOL' as a word -> EM_LAUGH priority 11", () => {
    GetEmotionsFromString("that is LOL", opts);
    const e = findEmotion(opts, EM_LAUGH);
    expect(e).not.toBeNull();
    expect(e!.m_intensity).toBe(1.0);
    expect(opts.m_priorities[0]).toBe(11);
  });
  it("'lol' (lowercase) still matches (CheckWord* is case-insensitive)", () => {
    GetEmotionsFromString("that is lol", opts);
    expect(findEmotion(opts, EM_LAUGH)).not.toBeNull();
  });
  it("'ROTFL' matches", () => {
    GetEmotionsFromString("ROTFL", opts);
    expect(findEmotion(opts, EM_LAUGH)).not.toBeNull();
  });
  it("'hehe' matches via FindString* (case-insensitive substring)", () => {
    GetEmotionsFromString("hehe that's great", opts);
    expect(findEmotion(opts, EM_LAUGH)).not.toBeNull();
  });
  it("'LOL' mid-word ('xLOL') does NOT match CheckWord*", () => {
    GetEmotionsFromString("xLOL", opts);
    // FindString*("HEHE") won't fire; CheckWord*("LOL") needs word boundary.
    expect(findEmotion(opts, EM_LAUGH)).toBeNull();
  });
});

describe("GetEmotionsFromString — HAPPY / SAD smileys", () => {
  it("':)' -> EM_HAPPY priority 10", () => {
    GetEmotionsFromString("hi :)", opts);
    const e = findEmotion(opts, EM_HAPPY);
    expect(e).not.toBeNull();
    expect(e!.m_intensity).toBe(1.0);
  });
  it("':-)' -> EM_HAPPY", () => {
    GetEmotionsFromString(":-) nice", opts);
    expect(findEmotion(opts, EM_HAPPY)).not.toBeNull();
  });
  it("':(' -> EM_SAD priority 10", () => {
    GetEmotionsFromString("oh :( sad", opts);
    expect(findEmotion(opts, EM_SAD)).not.toBeNull();
  });
  it("':-(' -> EM_SAD", () => {
    GetEmotionsFromString(":-( bummer", opts);
    expect(findEmotion(opts, EM_SAD)).not.toBeNull();
  });
});

describe("GetEmotionsFromString — COY", () => {
  it("';-)' -> EM_COY priority 10", () => {
    GetEmotionsFromString(";-) maybe", opts);
    expect(findEmotion(opts, EM_COY)).not.toBeNull();
  });
  it("';)' -> EM_COY (v2.5 added this rule)", () => {
    GetEmotionsFromString(";) sure", opts);
    expect(findEmotion(opts, EM_COY)).not.toBeNull();
  });
});

describe("GetEmotionsFromString — WAVE (sentence-start, case-insensitive)", () => {
  it("'Hello' at start -> EM_WAVE priority 5", () => {
    GetEmotionsFromString("Hello there", opts);
    const e = findEmotion(opts, EM_WAVE);
    expect(e).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(5);
  });
  it("'Hi' at start -> EM_WAVE priority 2", () => {
    GetEmotionsFromString("Hi", opts);
    expect(findEmotion(opts, EM_WAVE)).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(2);
  });
  it("'Howdy' at start -> EM_WAVE priority 5", () => {
    GetEmotionsFromString("Howdy partner", opts);
    expect(findEmotion(opts, EM_WAVE)).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(5);
  });
  it("'Bye' at start -> EM_WAVE priority 3", () => {
    GetEmotionsFromString("Bye now", opts);
    expect(findEmotion(opts, EM_WAVE)).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(3);
  });
  it("'Welcome' at start -> EM_WAVE priority 5", () => {
    GetEmotionsFromString("Welcome", opts);
    expect(findEmotion(opts, EM_WAVE)).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(5);
  });
  it("'hello' lowercase still matches (CheckStart* is case-insensitive)", () => {
    GetEmotionsFromString("hello there", opts);
    expect(findEmotion(opts, EM_WAVE)).not.toBeNull();
  });
  it("'Hellothere' does NOT match (CheckStart requires non-alnum after)", () => {
    GetEmotionsFromString("Hellothere", opts);
    expect(findEmotion(opts, EM_WAVE)).toBeNull();
  });
  it("WAVE fires at the start of EACH sentence, not just the first", () => {
    GetEmotionsFromString("Well. Hello there", opts);
    expect(findEmotion(opts, EM_WAVE)).not.toBeNull();
  });
});

describe("GetEmotionsFromString — POINTSELF (CheckStart* 'I' + CheckWord*)", () => {
  it("'I' at start -> EM_POINTSELF priority 3", () => {
    GetEmotionsFromString("I think so", opts);
    expect(findEmotion(opts, EM_POINTSELF)).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(3);
  });
  it("'i'm' as a word -> EM_POINTSELF priority 7 (overrides the weaker 3)", () => {
    // "I'm" -> CheckStart*("I")=3 AND CheckWord*("i'm")=7 -> priority wins 7.
    GetEmotionsFromString("I'm going", opts);
    const e = findEmotion(opts, EM_POINTSELF);
    expect(e).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(7);
  });
  it("'i am' as a word -> priority 7", () => {
    GetEmotionsFromString("i am here", opts);
    expect(findEmotion(opts, EM_POINTSELF)).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(7);
  });
});

describe("GetEmotionsFromString — POINTOTHER", () => {
  it("'You' at start -> EM_POINTOTHER priority 4", () => {
    GetEmotionsFromString("You are nice", opts);
    expect(findEmotion(opts, EM_POINTOTHER)).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(4);
  });
  it("'are you' as a word -> priority 8 (overrides 4)", () => {
    GetEmotionsFromString("how are you", opts);
    const e = findEmotion(opts, EM_POINTOTHER);
    expect(e).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(8);
  });
  it("'will you' as a word -> priority 8", () => {
    GetEmotionsFromString("will you go", opts);
    expect(findEmotion(opts, EM_POINTOTHER)).not.toBeNull();
    expect(opts.m_priorities[0]).toBe(8);
  });
});

describe("GetEmotionsFromString — multi-rule interaction + dedup", () => {
  it("a message can trigger multiple distinct emotions", () => {
    // "Hello LOL :)" -> WAVE + LAUGH + HAPPY (three separate emotion entries)
    GetEmotionsFromString("Hello LOL :)", opts);
    expect(findEmotion(opts, EM_WAVE)).not.toBeNull();
    expect(findEmotion(opts, EM_LAUGH)).not.toBeNull();
    expect(findEmotion(opts, EM_HAPPY)).not.toBeNull();
    expect(opts.m_nOpts).toBe(3);
  });
  it("same emotion from two rules keeps the HIGHER priority (OVERRIDEBYPRIORITY)", () => {
    // "I'm" -> CheckStart*("I")=3 fires first, then CheckWord*("i'm")=7 fires.
    // Both add EM_POINTSELF; the second has higher priority so it wins.
    GetEmotionsFromString("I'm sure", opts);
    const pointSelf = findEmotion(opts, EM_POINTSELF);
    expect(pointSelf).not.toBeNull();
    // Only ONE entry for EM_POINTSELF (dedup happened).
    let count = 0;
    for (let i = 0; i < opts.m_nOpts; i++) if (opts.m_emotions[i].m_emotion === EM_POINTSELF) count++;
    expect(count).toBe(1);
    expect(opts.m_priorities[0]).toBe(7);
  });
  it("plain text with no triggers produces zero opts", () => {
    GetEmotionsFromString("the quick brown fox jumps", opts);
    expect(opts.m_nOpts).toBe(0);
    expect(entries(opts)).toEqual([]);
  });
  it("empty string produces zero opts", () => {
    GetEmotionsFromString("", opts);
    expect(opts.m_nOpts).toBe(0);
  });
});

describe("InitializeEmotionRules is idempotent (no double-registration)", () => {
  it("calling init twice doesn't duplicate rules", () => {
    InitializeEmotionRules();
    InitializeEmotionRules();
    opts = new CEmotionOpts();
    GetEmotionsFromString("Hello", opts);
    // WAVE should still be exactly one entry.
    expect(opts.m_nOpts).toBe(1);
    expect(findEmotion(opts, EM_WAVE)).not.toBeNull();
  });
});