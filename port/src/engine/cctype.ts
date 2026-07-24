/**
 * ASCII ctype helpers — faithful to MSVC's `is*`/`to*` in the C locale.
 *
 * The engine's text matching (textpose.cpp) uses `isupper`/`islower`/
 * `isspace`/`ispunct`/`isdigit`/`isalnum`/`isprint` and `tolower`/`stricmp`/
 * `strnicmp`. These are locale-aware in C, but the engine's rule strings are
 * ASCII-only and the input text is treated as bytes. We use char-code checks
 * (NOT JS regex like `/\s/` or `.toUpperCase()`, which are Unicode-aware and
 * would diverge on non-ASCII whitespace / case-folding).
 *
 * Ranges follow the C locale (ASCII):
 *   space: 0x09-0x0D, 0x20   punct: the standard ASCII punct set   print: 0x20-0x7E
 *   upper: 0x41-0x5A         lower: 0x61-0x7A                      digit: 0x30-0x39
 *   alnum: digit | upper | lower
 *
 * All helpers take a char CODE (number) so call sites do `.charCodeAt(i)`
 * once and reuse — matching the C `*buff`/`*buff++` byte-deref pattern.
 */

export function isUpper(c: number): boolean { return c >= 0x41 && c <= 0x5a; }
export function isLower(c: number): boolean { return c >= 0x61 && c <= 0x7a; }
export function isDigit(c: number): boolean { return c >= 0x30 && c <= 0x39; }

// C isspace: 0x09 (HT), 0x0A (LF), 0x0B (VT), 0x0C (FF), 0x0D (CR), 0x20 (space)
export function isSpace(c: number): boolean {
  return c === 0x20 || (c >= 0x09 && c <= 0x0d);
}

// C isprint: 0x20-0x7E (printable ASCII incl. space)
export function isPrint(c: number): boolean { return c >= 0x20 && c <= 0x7e; }

// C isalnum: digit | upper | lower
export function isAlnum(c: number): boolean {
  return isDigit(c) || isUpper(c) || isLower(c);
}

// C ispunct: printable but not space and not alnum. ASCII punct = 0x21-0x2F,
// 0x3A-0x40, 0x5B-0x60, 0x7B-0x7E.
export function isPunct(c: number): boolean {
  if (!isPrint(c) || isSpace(c) || isAlnum(c)) return false;
  return true;
}

// tolower (C) — ASCII only; leaves non-upper unchanged.
export function toLowerChar(c: number): number {
  return isUpper(c) ? c + 0x20 : c;
}

// ToLower (textpose.cpp:105) — returns a lowered copy of the whole string.
// C: per-char tolower over a strdup'd buffer. We build a new string.
export function toLower(s: string): string {
  let out = "";
  for (let i = 0; i < s.length; i++) out += String.fromCharCode(toLowerChar(s.charCodeAt(i)));
  return out;
}

// stricmp — case-insensitive ASCII compare. Returns 0 if equal, <0 if a<b,
// >0 if a>b (lexicographic on the lowercased chars, matching C).
export function stricmp(a: string, b: string): number {
  const n = Math.min(a.length, b.length);
  for (let i = 0; i < n; i++) {
    const ca = toLowerChar(a.charCodeAt(i));
    const cb = toLowerChar(b.charCodeAt(i));
    if (ca !== cb) return ca - cb;
  }
  return a.length - b.length;
}

// strnicmp — case-insensitive ASCII compare of the first `len` chars.
export function strnicmp(a: string, b: string, len: number): number {
  const n = Math.min(len, a.length, b.length);
  for (let i = 0; i < n; i++) {
    const ca = toLowerChar(a.charCodeAt(i));
    const cb = toLowerChar(b.charCodeAt(i));
    if (ca !== cb) return ca - cb;
  }
  if (len <= Math.min(a.length, b.length)) return 0;
  // reached `len` on the shorter string: compare by effective length
  return a.length >= len ? 0 : a.length - b.length;
}