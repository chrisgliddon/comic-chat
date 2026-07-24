#!/usr/bin/env node
// critic.mjs — completeness critic pass for the oracle corpus.
//
// For every Tier-1/Tier-3 behavior in the plan doc inventory, assert >=1
// corpus case exercises it. Reports gaps.
//
//   node critic.mjs [--corpus <corpusDir>]
//
// Exit 0 = all covered, 1 = gaps found.

import fs from "node:fs";
import path from "node:path";

const corpusDir = process.argv.includes("--corpus")
  ? process.argv[process.argv.indexOf("--corpus") + 1] || "oracle/corpus"
  : "oracle/corpus";

// The inventory from plan doc §5 Tier 3 + Tier 1, expressed as checks.
// Each check has: id, description, and a predicate over the corpus.
const checks = [
  // Tier-3 #1: Panel membership + new-panel boolean
  { id: "T3.1.panel-membership", desc: "Panel membership + new-panel boolean",
    check: (cases) => cases.some(c => c.messages?.some(m => m.page?.panels?.length > 0)) },

  // Tier-3 #1: panel-break triggers
  { id: "T3.1.break-5balloons", desc: "Panel break: >=5 balloons",
    check: (cases) => cases.some(c => c.messages?.length >= 5) },
  { id: "T3.1.break-speaker-present", desc: "Panel break: speaker-already-present",
    check: (cases) => cases.some(c => {
      const msgs = c.messages || [];
      return msgs.some((m, i) => i > 0 && msgs[i-1].speakerId === m.speakerId);
    }) },
  { id: "T3.1.break-action", desc: "Panel break: ACTION mode",
    check: (cases) => cases.some(c => c.messages?.some(m => m.mode === 4)) },
  { id: "T3.1.break-brk", desc: "Panel break: <Brk> token",
    check: (cases) => cases.some(c => c.messages?.some(m => m.text?.includes("<Brk>"))) },
  { id: "T3.1.break-overflow", desc: "Panel break: overflow spill (long message)",
    check: (cases) => cases.some(c => c.messages?.some(m => (m.text?.length || 0) > 150)) },

  // Tier-3 #2: avatar order/flip/historesis
  { id: "T3.2.avatar-order", desc: "Left-to-right avatar order in bodies",
    check: (cases) => cases.some(c => c.messages?.some(m => m.page?.panels?.some(p => p.bodies?.length >= 2))) },
  { id: "T3.2.avatar-flip", desc: "Per-body m_flip",
    check: (cases) => cases.some(c => c.messages?.some(m => m.page?.panels?.some(p => p.bodies?.some(b => "flip" in b)))) },

  // Tier-3 #3: zoom + body rects
  { id: "T3.3.body-bbox", desc: "Per-body bbox",
    check: (cases) => cases.some(c => c.messages?.some(m => m.avatarStates && Object.values(m.avatarStates).some(a => a.body?.bbox))) },

  // Tier-3 #4: CFormatInfo line breaks
  { id: "T3.4.formatinfo", desc: "CFormatInfo line-break output",
    check: (cases) => cases.some(c => c.messages?.some(m => m.page?.panels?.some(p => p.balloons?.some(b => b.formatInfo)))) },

  // Tier-3 #5: balloon bbox/trueBox/routeRgn/tail
  { id: "T3.5.balloon-bbox", desc: "Balloon bbox/trueBox/routeRgn",
    check: (cases) => cases.some(c => c.messages?.some(m => m.page?.panels?.some(p => p.balloons?.some(b => b.trueBox && b.routeRgn)))) },

  // Tier-3 #6: spline control points
  { id: "T3.6.spline-cps", desc: "Balloon outline spline control points",
    check: (cases) => cases.some(c => c.messages?.some(m => m.page?.panels?.some(p => p.balloons?.some(b => b.spline?.cps)))) },

  // Tier-3 #7: avatar hidden state
  { id: "T3.7.avatar-state", desc: "Avatar hidden state (lastBody/Face/Torso, flags)",
    check: (cases) => cases.some(c => c.messages?.some(m => m.avatarStates && Object.values(m.avatarStates).some(a => a.body))) },

  // Modes coverage
  { id: "mode.say", desc: "SAY mode (0)",
    check: (cases) => cases.some(c => c.messages?.some(m => m.mode === 0)) },
  { id: "mode.whisper", desc: "WHISPER mode (1)",
    check: (cases) => cases.some(c => c.messages?.some(m => m.mode === 1)) },
  { id: "mode.think", desc: "THINK mode (2)",
    check: (cases) => cases.some(c => c.messages?.some(m => m.mode === 2)) },
  { id: "mode.action", desc: "ACTION mode (4)",
    check: (cases) => cases.some(c => c.messages?.some(m => m.mode === 4)) },

  // Speaker count coverage
  { id: "speakers.1", desc: "1 speaker",
    check: (cases) => cases.some(c => (c.avatars?.length || 0) === 1) },
  { id: "speakers.2", desc: "2 speakers",
    check: (cases) => cases.some(c => (c.avatars?.length || 0) === 2) },
  { id: "speakers.3+", desc: "3+ speakers",
    check: (cases) => cases.some(c => (c.avatars?.length || 0) >= 3) },
  { id: "speakers.6", desc: "6 speakers",
    check: (cases) => cases.some(c => (c.avatars?.length || 0) === 6) },

  // Edge cases
  { id: "edge.empty", desc: "Empty message",
    check: (cases) => cases.some(c => c.messages?.some(m => m.text === "")) },
  { id: "edge.unicode", desc: "Unicode text",
    check: (cases) => cases.some(c => c.messages?.some(m => /[\u00c0-\u017f]/.test(m.text || ""))) },
  { id: "edge.caps", desc: "ALL CAPS (SHOUT trigger)",
    check: (cases) => cases.some(c => c.messages?.some(m => /[A-Z]{5,}/.test(m.text || ""))) },

  // Multi-panel pagination
  { id: "T3.multi-page", desc: "Multi-panel pagination (allPages with >1 page)",
    check: (cases) => cases.some(c => c.messages?.some(m => (m.allPages?.length || 0) > 1)) },
];

function loadCorpus(dir) {
  if (!fs.existsSync(dir)) return [];
  const cases = [];
  const entries = fs.readdirSync(dir).filter((n) => /^\d+$/.test(n)).sort();
  for (const c of entries) {
    const inp = path.join(dir, c, "inputs.json");
    const exp = path.join(dir, c, "expected.json");
    const entry = { caseId: c, inputs: null, expected: null, messages: [] };
    if (fs.existsSync(inp)) entry.inputs = JSON.parse(fs.readFileSync(inp, "utf8"));
    // Use inputs for message-level checks (inputs always exist)
    if (entry.inputs) {
      entry.messages = entry.inputs.messages || [];
      entry.avatars = entry.inputs.avatars || [];
    }
    // If expected exists, use it for state-level checks
    if (fs.existsSync(exp)) {
      entry.expected = JSON.parse(fs.readFileSync(exp, "utf8"));
      entry.messages = entry.expected.messages || entry.messages;
    }
    cases.push(entry);
  }
  return cases;
}

function main() {
  const corpus = loadCorpus(corpusDir);
  if (corpus.length === 0) {
    console.error(`No corpus cases found in ${corpusDir}`);
    process.exit(1);
  }

  // Run checks against inputs (for pre-generation checks) or expected (for post)
  const inputsOnly = corpus.filter(c => c.inputs && !c.expected);
  const withExpected = corpus.filter(c => c.expected);
  const checkSource = withExpected.length > 0 ? withExpected : inputsOnly;

  // For input-level checks, use inputs; for state-level, use expected
  const inputCases = corpus.filter(c => c.inputs);
  const expectedCases = withExpected;

  const passed = [];
  const gaps = [];

  for (const chk of checks) {
    // Use expected cases for state-level checks, input cases for message-level
    const source = chk.id.startsWith("T3.") && chk.id !== "T3.multi-page" ? (expectedCases.length > 0 ? expectedCases : inputCases) : inputCases;
    const ok = chk.check(source);
    if (ok) {
      passed.push(chk.id);
    } else {
      gaps.push(chk);
    }
  }

  console.log(`Completeness critic: ${passed.length}/${checks.length} checks passed, ${gaps.length} gaps.`);
  if (gaps.length > 0) {
    console.log("\nGAPS (need more corpus cases):");
    for (const g of gaps) {
      console.log(`  ${g.id}: ${g.desc}`);
    }
    process.exit(1);
  }
  console.log("All Tier-1/3 behaviors covered.");
  process.exit(0);
}

main();