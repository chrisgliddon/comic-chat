/**
 * Textpose probe dumper — the TS mirror of `CaptureTextpose` in
 * `oracle/harness/oracleharness.cpp`. Runs the SAME probe battery through
 * `GetEmotionsFromString` and returns a JSON-identical structure so the
 * oracle golden test can diff the two byte-for-byte.
 *
 * KEEP THE PROBE LIST IN SYNC with `CaptureTextpose` in the C++ harness.
 * Both lists must contain the same strings in the same order.
 */
import { CEmotionOpts } from "../core/emotionopts.js";

export interface TextposeProbeResult {
  text: string;
  opts: { emotion: string; intensity: string; priority: number }[];
}
export interface TextposeDump {
  probes: TextposeProbeResult[];
}

// The probe battery — identical to the C++ CaptureTextpose probes[] array.
const PROBES: readonly string[] = [
  "HELLO",
  "what!!!",
  "hello",
  "that is LOL",
  "that is lol",
  "ROTFL",
  "hehe that's great",
  "xLOL",
  "hi :)",
  ":-) nice",
  "oh :( sad",
  ":-( bummer",
  ";-) maybe",
  ";) sure",
  "Hello there",
  "Hi",
  "Howdy partner",
  "Bye now",
  "Welcome",
  "hello there",
  "Hellothere",
  "Well. Hello there",
  "I think so",
  "I'm going",
  "i am here",
  "You are nice",
  "how are you",
  "will you go",
  "Hello LOL :)",
  "I'm sure",
  "the quick brown fox jumps",
  "",
];

export function dumpTextposeProbes(
  getEmotions: (str: string, opts: CEmotionOpts) => void,
  init: () => void,
): TextposeDump {
  init();
  const probes: TextposeProbeResult[] = [];
  for (const text of PROBES) {
    const opts = new CEmotionOpts();
    getEmotions(text, opts);
    const optsOut: TextposeProbeResult["opts"] = [];
    for (let j = 0; j < opts.m_nOpts; j++) {
      optsOut.push({
        // Match the C sprintf "%.17g" format: shortest round-trip float.
        // JS Number.toString() already gives shortest round-trip for doubles,
        // which is what %.17g effectively yields after trailing-zero strip.
        emotion: String(opts.m_emotions[j].m_emotion),
        intensity: String(opts.m_emotions[j].m_intensity),
        priority: opts.m_priorities[j],
      });
    }
    probes.push({ text, opts: optsOut });
  }
  return { probes };
}