#!/usr/bin/env node
// validate.mjs — validate oracle corpus files against the JSON schemas.
//
//   node validate.mjs <inputs.json> [expected.json]
//   node validate.mjs --corpus <corpusDir>
//
// Exit 0 = valid, 1 = invalid, 2 = usage error.
// Uses ajv if available; falls back to structural validation.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

let Ajv = null;
try {
  // The schemas use draft 2020-12 ($defs, not definitions), so import the
  // 2020-12-capable entry point. Falls back to the default export on older
  // ajv versions or when the 2020 module is absent.
  try {
    const ajv2020 = await import("ajv/dist/2020.js");
    Ajv = ajv2020.default;
  } catch {
    const ajvMod = await import("ajv");
    Ajv = ajvMod.default;
  }
} catch {
  // ajv not installed — fall back to basic structural checks
}

function loadSchema(name) {
  const schemaDir = path.join(__dirname, "..", "schema");
  return JSON.parse(fs.readFileSync(path.join(schemaDir, name), "utf8"));
}

function validateWithAjv(schemaName, file, data) {
  const schema = loadSchema(schemaName);
  const ajv = new Ajv({ allErrors: true, strict: false });
  const validate = ajv.compile(schema);
  const valid = validate(data);
  if (!valid) {
    console.error(`INVALID: ${file}`);
    for (const err of validate.errors) {
      console.error(`  ${err.instancePath || "$"}: ${err.message}`);
    }
    return false;
  }
  return true;
}

// Basic structural validation (fallback when ajv is not available)
function validateBasic(schemaName, file, data) {
  const errors = [];
  if (schemaName === "inputs.schema.json") {
    if (typeof data.seed !== "number") errors.push("seed: must be integer");
    if (!Array.isArray(data.messages)) errors.push("messages: must be array");
    if (data.messages) {
      for (let i = 0; i < data.messages.length; i++) {
        const m = data.messages[i];
        if (typeof m.speakerId !== "number") errors.push(`messages[${i}].speakerId: must be integer`);
        if (typeof m.text !== "string") errors.push(`messages[${i}].text: must be string`);
      }
    }
  } else if (schemaName === "expected.schema.json") {
    if (typeof data.seed !== "number") errors.push("seed: must be integer");
    if (!Array.isArray(data.messages)) errors.push("messages: must be array");
  }
  if (errors.length > 0) {
    console.error(`INVALID: ${file}`);
    for (const e of errors) console.error(`  ${e}`);
    return false;
  }
  return true;
}

function validateFile(file, schemaName) {
  const data = JSON.parse(fs.readFileSync(file, "utf8"));
  if (Ajv) return validateWithAjv(schemaName, file, data);
  return validateBasic(schemaName, file, data);
}

function main() {
  const [, , ...args] = process.argv;
  if (args.length === 0) {
    console.error("usage: validate.mjs <inputs.json> [expected.json]");
    console.error("       validate.mjs --corpus <corpusDir>");
    process.exit(2);
  }

  let allValid = true;

  if (args[0] === "--corpus") {
    const dir = args[1];
    if (!dir) { console.error("missing corpus dir"); process.exit(2); }
    const cases = fs.readdirSync(dir).filter((n) => /^\d+$/.test(n)).sort();
    for (const c of cases) {
      const inp = path.join(dir, c, "inputs.json");
      const exp = path.join(dir, c, "expected.json");
      if (fs.existsSync(inp)) {
        if (!validateFile(inp, "inputs.schema.json")) allValid = false;
      }
      if (fs.existsSync(exp)) {
        if (!validateFile(exp, "expected.schema.json")) allValid = false;
      }
    }
  } else {
    const inp = args[0];
    if (!validateFile(inp, "inputs.schema.json")) allValid = false;
    if (args[1] && fs.existsSync(args[1])) {
      if (!validateFile(args[1], "expected.schema.json")) allValid = false;
    }
  }

  if (allValid) {
    console.log("VALID");
    process.exit(0);
  }
  process.exit(1);
}

main();