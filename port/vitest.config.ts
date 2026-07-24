import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    include: ["test/**/*.test.ts"],
    environment: "node",
    // Determinism: fixed root for file-path fixtures, no parallelism surprises
    // in numeric property tests.
    pool: "forks",
    fileParallelism: false,
  },
  resolve: {
    alias: {
      "@port": new URL("./src", import.meta.url).pathname,
    },
  },
});