#!/usr/bin/env node
// diff-dumps.mjs — compare two oracle dump JSON files (or directories of
// NNN/expected.json cases) and report the FIRST divergent path per case.
//
//   node diff-dumps.mjs a.json b.json
//   node diff-dumps.mjs corpusDirA corpusDirB
//
// Exit code 0 = identical, 1 = divergent, 2 = usage/IO error.
// The first divergent message + field is the signal (plan doc §5 Tier 4).

import fs from "node:fs";
import path from "node:path";

function typeOf(v) {
  if (v === null) return "null";
  if (Array.isArray(v)) return "array";
  return typeof v;
}

// Returns the first divergence as {path, a, b} or null if equal.
function firstDiff(a, b, p = "$") {
  const ta = typeOf(a), tb = typeOf(b);
  if (ta !== tb) return { path: p, a: `<${ta}>`, b: `<${tb}>` };
  if (ta === "array") {
    const n = Math.min(a.length, b.length);
    for (let i = 0; i < n; i++) {
      const d = firstDiff(a[i], b[i], `${p}[${i}]`);
      if (d) return d;
    }
    if (a.length !== b.length)
      return { path: `${p}.length`, a: a.length, b: b.length };
    return null;
  }
  if (ta === "object") {
    const keys = [...new Set([...Object.keys(a), ...Object.keys(b)])];
    for (const k of keys) {
      if (!(k in a)) return { path: `${p}.${k}`, a: "<missing>", b: JSON.stringify(b[k]) };
      if (!(k in b)) return { path: `${p}.${k}`, a: JSON.stringify(a[k]), b: "<missing>" };
      const d = firstDiff(a[k], b[k], `${p}.${k}`);
      if (d) return d;
    }
    return null;
  }
  if (Object.is(a, b)) return null;
  return { path: p, a: JSON.stringify(a), b: JSON.stringify(b) };
}

function loadJson(file) {
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

function diffFilePair(fa, fb, label) {
  // Byte-identical is the strong check; JSON-equal is the diagnostic.
  const rawA = fs.readFileSync(fa), rawB = fs.readFileSync(fb);
  if (rawA.equals(rawB)) return null;
  const d = firstDiff(loadJson(fa), loadJson(fb));
  if (d) return { label, ...d };
  return { label, path: "<bytes>", a: "files differ in bytes but are JSON-equal", b: "(formatting/nondeterministic emit?)" };
}

function main() {
  const [, , aArg, bArg] = process.argv;
  if (!aArg || !bArg) {
    console.error("usage: diff-dumps.mjs <a.json|dirA> <b.json|dirB>");
    process.exit(2);
  }
  const statA = fs.statSync(aArg), statB = fs.statSync(bArg);
  const diffs = [];
  if (statA.isDirectory() && statB.isDirectory()) {
    const casesA = fs.readdirSync(aArg).filter((n) => /^\d+$/.test(n)).sort();
    const casesB = new Set(fs.readdirSync(bArg).filter((n) => /^\d+$/.test(n)));
    for (const c of casesA) {
      if (!casesB.has(c)) { diffs.push({ label: c, path: "<case>", a: "present", b: "missing" }); continue; }
      const fa = path.join(aArg, c, "expected.json");
      const fb = path.join(bArg, c, "expected.json");
      if (!fs.existsSync(fa) || !fs.existsSync(fb)) {
        diffs.push({ label: c, path: "<expected.json>", a: String(fs.existsSync(fa)), b: String(fs.existsSync(fb)) });
        continue;
      }
      const d = diffFilePair(fa, fb, c);
      if (d) diffs.push(d);
    }
    for (const c of casesB) {
      if (!casesA.includes(c)) diffs.push({ label: c, path: "<case>", a: "missing", b: "present" });
    }
  } else {
    const d = diffFilePair(aArg, bArg, path.basename(aArg));
    if (d) diffs.push(d);
  }

  if (diffs.length === 0) {
    console.log("IDENTICAL");
    process.exit(0);
  }
  for (const d of diffs) {
    console.log(`DIVERGENT ${d.label}: ${d.path}`);
    console.log(`  a: ${d.a}`);
    console.log(`  b: ${d.b}`);
  }
  console.log(`${diffs.length} divergent case(s)`);
  process.exit(1);
}

main();
