/**
 * Port of `textpose.cpp` — text → emotion rule matching.
 *
 * Sources:
 *   - `v1.0-pre-modern/textpose.cpp` (reference; the rule-matching logic is
 *     byte-identical to the v2.5 oracle tree).
 *   - `v2.5-beta-1-modern/chat.rc:2290-2304` — the FROZEN rule strings the
 *     oracle loads via `LoadString(ID_RULE_*)`. The port embeds these as
 *     constants so it doesn't depend on Windows resources. The v1.0 and v2.5
 *     rule strings differ (v2.5 adds HEHE→LAUGH, ;→COY, and makes LAUGH
 *     case-insensitive); we use the **v2.5** set since that's the golden tree.
 *
 * This is Tier-1 #1 (plan doc): the highest-value single unit. The output
 * (`CEmotionOpts`) feeds `GetBodyFromEmotion` (avatar.cpp) which selects the
 * avatar pose. The oracle corpus captures the result as `faceEmotion`/
 * `torsoEmotion`, but no corpus case currently exercises a non-zero emotion
 * dump — so the two-agent check here is hand-computed golden expectations
 * against the frozen rule table (see test/engine/textpose.test.ts). When the
 * oracle gains a Tier-1 #1 standalone dump mode (LEDGER TODO), the port's
 * output should be diffed against it directly.
 *
 * RULEBOOK notes:
 *   - §Value-Types: `CString`→`string`, `CPtrList`→`STRINGUNIT[]`.
 *   - §Diagnostics: `ASSERT`/`TRACE` dropped (none here).
 *   - C `is*`/`str*` functions are ASCII-only in the engine's usage; we use
 *     char-code checks (NOT `/\s/` etc., which are Unicode-aware and would
 *     diverge on non-ASCII whitespace). See `cctype.ts` for the helpers.
 *   - `ToLower` produces a lowered copy for case-insensitive matching — the
 *     engine does `strdup` + per-char `tolower`; we do `string` + char map.
 *   - Rule strengths are integers; priorities are 0-255 (UCHAR).
 */
import { CEmotionOpts } from "../core/emotionopts.js";
import {
  EM_ANGRY,
  EM_BORED,
  EM_COY,
  EM_HAPPY,
  EM_LAUGH,
  EM_POINTOTHER,
  EM_POINTSELF,
  EM_SAD,
  EM_SCARED,
  EM_SHOUT,
  EM_WAVE,
} from "../core/emotion.js";
import {
  isAlnum,
  isDigit,
  isLower,
  isPunct,
  isPrint,
  isSpace,
  isUpper,
  stricmp,
  strnicmp,
  toLower,
} from "./cctype.js";

// ---------------------------------------------------------------------------
// STRINGUNIT (textpose.cpp:213-219) — one compiled rule.
// ---------------------------------------------------------------------------
interface STRINGUNIT {
  arg: string;
  length: number;
  strength: number;
  emotion: number;
  caseSensitive: boolean;
}

function StringUnit(
  emotion: number,
  arg: string,
  strength: number,
  caseSensitive: boolean,
): STRINGUNIT {
  return {
    emotion,
    arg: caseSensitive ? arg : toLower(arg),
    length: arg.length,
    strength,
    caseSensitive,
  };
}

// ---------------------------------------------------------------------------
// The frozen rule table (v2.5-beta-1-modern/chat.rc:2290-2304).
// Loaded in the C engine via InitializeEmotionRules() → LoadString(ID_RULE_*)
// → LoadCompositeRule → LoadSingleRule → RegisterRule. The port inlines the
// result of that load so there's no resource dependency. ANGRY/SCARED/BORED
// are empty strings in the shipped rules (no matchers) — kept for parity.
// ---------------------------------------------------------------------------
const RULE_TABLE: ReadonlyArray<{ emotion: number; rule: string }> = [
  { emotion: EM_SHOUT, rule: `AllCaps("");9\nFindString("!!!");9` },
  { emotion: EM_LAUGH, rule: `CheckWord*("ROTFL");11\nCheckWord*("LOL");11\nFindString*("HEHE");11` },
  { emotion: EM_HAPPY, rule: `FindString(":)");10\nFindString(":-)");10` },
  { emotion: EM_SAD, rule: `FindString(":(");10\nFindString(":-(");10` },
  { emotion: EM_POINTOTHER, rule: `CheckStart*("You");4\nCheckWord*("are you");8\nCheckWord*("will you");8\nCheckWord*("did you");8\nCheckWord*("aren't you");8\nCheckWord*("don't you");8` },
  { emotion: EM_POINTSELF, rule: `CheckStart*("I");3\nCheckWord*("i'm");7\nCheckWord*("i will");7\nCheckWord*("i'll");7\nCheckWord*("i am");7` },
  { emotion: EM_WAVE, rule: `CheckStart*("Hi");2\nCheckStart*("Bye");3\nCheckStart*("Hello");5\nCheckStart*("Welcome");5\nCheckStart*("Howdy");5` },
  { emotion: EM_COY, rule: `FindString(";-)");10\nFindString(";)");10` },
  { emotion: EM_ANGRY, rule: `` },
  { emotion: EM_SCARED, rule: `` },
  { emotion: EM_BORED, rule: `` },
];

// ---------------------------------------------------------------------------
// Rule compiler — the RegisterRule / LoadSingleRule / LoadCompositeRule trio.
// ---------------------------------------------------------------------------

// The three rule lists (textpose.cpp:207-209). Module-level state, matching
// the C static globals. `InitializeEmotionRules()` populates them.
const generalRules: STRINGUNIT[] = [];
const wordRules: STRINGUNIT[] = [];
const sentenceRules: STRINGUNIT[] = [];
let capsStrength = 0; // textpose.cpp:210 — 0 means AllCaps rule inactive
let capsEmotion = 0;

function AddToGeneral(rule: STRINGUNIT): void { generalRules.push(rule); }
function AddToWord(rule: STRINGUNIT): void { wordRules.push(rule); }
function AddToSentence(rule: STRINGUNIT): void { sentenceRules.push(rule); }

// RegisterRule (textpose.cpp:244) — dispatch by function name.
function RegisterRule(emotion: number, fn: string, arg: string, strength: number): void {
  if (stricmp(fn, "AllCaps") === 0) {
    capsStrength = strength;
    capsEmotion = emotion;
  } else if (stricmp(fn, "FindString") === 0) {
    AddToGeneral(StringUnit(emotion, arg, strength, true));
  } else if (stricmp(fn, "FindString*") === 0) {
    AddToGeneral(StringUnit(emotion, arg, strength, false));
  } else if (stricmp(fn, "CheckWord") === 0) {
    AddToWord(StringUnit(emotion, arg, strength, true));
  } else if (stricmp(fn, "CheckWord*") === 0) {
    AddToWord(StringUnit(emotion, arg, strength, false));
  } else if (stricmp(fn, "CheckStart") === 0) {
    AddToSentence(StringUnit(emotion, arg, strength, true));
  } else if (stricmp(fn, "CheckStart*") === 0) {
    AddToSentence(StringUnit(emotion, arg, strength, false));
  }
}

// ReadString (textpose.cpp:154) — read a quoted string from the rule text.
// Returns {rest, str}. The C version mutates a `char *buff` out-param; we
// return the extracted string and the remaining unparsed tail.
function ReadString(s: string): { rest: string; str: string } {
  const firstQuote = s.indexOf('"');
  if (firstQuote < 0) return { rest: s, str: "" };
  const secondQuote = s.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return { rest: s, str: "" };
  const str = s.slice(firstQuote + 1, secondQuote);
  return { rest: s.slice(secondQuote + 1), str };
}

// LoadSingleRule (textpose.cpp:172) — parse one "Function("arg");strength"
// entry. Returns {rest, ok} where rest is the unparsed tail (after the
// trailing newline(s)) and ok is false at end-of-string.
function LoadSingleRule(emotion: number, start: string): { rest: string; ok: boolean } {
  let sptr = start;
  // proceed to start (skip non-printable) — C: `while(!isprint(*sptr) && *sptr) sptr++`
  let i = 0;
  while (i < sptr.length && !isPrint(sptr.charCodeAt(i))) i++;
  sptr = sptr.slice(i);
  if (sptr.length === 0) return { rest: sptr, ok: false };

  // parse keyword (up to '(')
  let fnEnd = 0;
  while (fnEnd < sptr.length && sptr.charCodeAt(fnEnd) !== 40 /*'('*/) fnEnd++;
  const fn = sptr.slice(0, fnEnd);
  sptr = sptr.slice(fnEnd);
  if (sptr.length === 0) return { rest: sptr, ok: false };

  // skip '(', read arg string
  sptr = sptr.slice(1); // increment past (
  const { rest: afterArg, str: arg } = ReadString(sptr);
  sptr = afterArg;
  // skip to ';'
  let semi = 0;
  while (semi < sptr.length && sptr.charCodeAt(semi) !== 59 /*';'*/) semi++;
  sptr = sptr.slice(semi);
  if (sptr.length === 0) return { rest: sptr, ok: false };

  // skip ';', parse strength (digits only)
  sptr = sptr.slice(1); // increment past ;
  let strengthStr = "";
  let j = 0;
  // C: `while (*sptr != '\n' && *sptr) if (isdigit(*sptr)) *strPtr++ = *sptr++;`
  // — note the `if isdigit`: non-digit chars before the newline are SKIPPED
  // (not consumed into strengthStr). We replicate: advance j past everything
  // until newline, but only digits are collected.
  while (j < sptr.length && sptr.charCodeAt(j) !== 10 /*'\n'*/) {
    if (isDigit(sptr.charCodeAt(j))) strengthStr += sptr[j];
    j++;
  }
  sptr = sptr.slice(j);
  const strength = parseInt(strengthStr, 10) || 0;
  // skip trailing newlines
  while (sptr.length > 0 && sptr.charCodeAt(0) === 10 /*'\n'*/) sptr = sptr.slice(1);

  RegisterRule(emotion, fn, arg, strength);
  return { rest: sptr, ok: sptr.length > 0 };
}

// LoadCompositeRule (textpose.cpp:141) — loop LoadSingleRule until it returns false.
function LoadCompositeRule(emotion: number, rule: string): void {
  let rest = rule;
  while (true) {
    const r = LoadSingleRule(emotion, rest);
    rest = r.rest;
    if (!r.ok) break;
  }
}

// InitializeEmotionRules (textpose.cpp:130) — compile the frozen rule table.
// Idempotent: clears the lists first so repeated calls don't accumulate
// (the C version is called once at startup; the port guards re-entry for tests).
export function InitializeEmotionRules(): void {
  generalRules.length = 0;
  wordRules.length = 0;
  sentenceRules.length = 0;
  capsStrength = 0;
  capsEmotion = 0;
  for (const { emotion, rule } of RULE_TABLE) {
    LoadCompositeRule(emotion, rule);
  }
}

// DestroyEmotionRules (textpose.cpp:324) — no-op in the port (GC handles it).
export function DestroyEmotionRules(): void {
  generalRules.length = 0;
  wordRules.length = 0;
  sentenceRules.length = 0;
  capsStrength = 0;
  capsEmotion = 0;
}

// ---------------------------------------------------------------------------
// Matchers
// ---------------------------------------------------------------------------

// CheckForUppers (textpose.cpp:25) — true if buff has no lowercase and >1 upper.
export function CheckForUppers(buff: string): boolean {
  let nUppers = 0;
  for (let i = 0; i < buff.length; i++) {
    const c = buff.charCodeAt(i);
    if (isLower(c)) return false;
    if (isUpper(c)) nUppers++;
  }
  return nUppers > 1;
}

// CheckWord (textpose.cpp:36) — true if `substr` appears in `buff` as a whole
// word (preceded by start-or-whitespace, followed by end/whitespace/punct).
// C uses `isspace`/`ispunct` for the boundary tests; we use the ASCII helpers.
export function CheckWord(buff: string, substr: string): boolean {
  let loc = 0;
  while (true) {
    const found = buff.indexOf(substr, loc);
    if (found < 0) return false;
    // starts a word: at buff start, or preceded by whitespace
    const startsWord = found === 0 || isSpace(buff.charCodeAt(found - 1));
    if (startsWord) {
      const afterIdx = found + substr.length;
      const after = afterIdx >= buff.length ? 0 : buff.charCodeAt(afterIdx);
      if (!after || isSpace(after) || isPunct(after)) return true;
    }
    loc = found + 1; // C: loc++ (advance one char past the match)
  }
}

// StartCompare2 (textpose.cpp:263) — strncmp(sent, sub, len)==0 && !isalnum(sent[len]).
// Case-sensitive variant used by GetEmotionsFromString for CheckStart.
function StartCompare2(sent: string, sub: string, len: number): boolean {
  if (sent.length < len) return false;
  if (sent.slice(0, len) !== sub) return false;
  const after = sent.charCodeAt(len);
  return !isAlnum(after); // !isalnum(0) is true (end of string) — matches C
}

// GetNextSentenceStart (textpose.cpp:98) — find the next sentence start after
// the current position (skip past a sentence terminator and leading punct/space).
// Returns -1 if none (C returns NULL).
function GetNextSentenceStart(buff: string, from: number): number {
  // strpbrk(buff, ".!?") from `from`
  let i = from;
  while (i < buff.length && ".!?".indexOf(buff[i]) < 0) i++;
  if (i >= buff.length) return -1;
  // skip the terminator + any punct/space
  i++;
  while (i < buff.length && (isPunct(buff.charCodeAt(i)) || isSpace(buff.charCodeAt(i)))) i++;
  return i >= buff.length ? -1 : i;
}

// ---------------------------------------------------------------------------
// GetEmotionsFromString (textpose.cpp:267) — the main entry point.
// ---------------------------------------------------------------------------
export function GetEmotionsFromString(str: string, emOpts: CEmotionOpts): void {
  const buff = str;
  const lower = toLower(buff);
  emOpts.m_nOpts = 0;

  // check for uppers (AllCaps rule)
  if (capsStrength !== 0 && CheckForUppers(buff)) {
    emOpts.Add(capsEmotion, 1.0, capsStrength);
  }

  // check generals (FindString / FindString*)
  for (const unit of generalRules) {
    if (unit.caseSensitive) {
      if (buff.indexOf(unit.arg) >= 0) emOpts.Add(unit.emotion, 1.0, unit.strength);
    } else if (lower.indexOf(unit.arg) >= 0) {
      emOpts.Add(unit.emotion, 1.0, unit.strength);
    }
  }

  // check words (CheckWord / CheckWord*)
  for (const unit of wordRules) {
    if (unit.caseSensitive) {
      if (CheckWord(buff, unit.arg)) emOpts.Add(unit.emotion, 1.0, unit.strength);
    } else if (CheckWord(lower, unit.arg)) {
      emOpts.Add(unit.emotion, 1.0, unit.strength);
    }
  }

  // check sentences (CheckStart / CheckStart*) — at the start of each sentence
  let bptr = 0;
  while (bptr < buff.length && isSpace(buff.charCodeAt(bptr))) bptr++; // prune leading ws
  while (bptr >= 0 && bptr < buff.length) {
    for (const unit of sentenceRules) {
      if (unit.caseSensitive) {
        if (StartCompare2(buff.slice(bptr), unit.arg, unit.length))
          emOpts.Add(unit.emotion, 1.0, unit.strength);
      } else if (StartCompare2(lower.slice(bptr), unit.arg, unit.length)) {
        emOpts.Add(unit.emotion, 1.0, unit.strength);
      }
    }
    bptr = GetNextSentenceStart(buff, bptr);
  }
}

// ChatPreSendText (textpose.cpp:118) — the engine's caller. Ported as a thin
// wrapper; the avatar lookup (`GetAvatar`/`MyAvatar`/`GetBodyFromEmotion`) is
// Phase 4 avatar.cpp territory. Exposed here so the call site is documented.
// The `emo` global (textpose.cpp:116) is module-level in C; the port takes an
// explicit opts arg to avoid shared mutable state across concurrent replays.
export { InitializeEmotionRules as init };