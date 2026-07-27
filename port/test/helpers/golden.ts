/**
 * Golden-presence helper for the differential oracle tests.
 *
 * A differential test whose golden is missing is worse than a failing one:
 * `it.skipIf` reports green, so a wrong path or an unfrozen dump looks like a
 * passing referee. Locally that skip is deliberate (you may not have run the
 * Windows harness yet); in CI it never is — the goldens are committed, so a
 * missing one means the path is wrong or the file was dropped.
 */
import { existsSync } from "node:fs";

export function hasGoldenOrThrowInCI(goldenPath: string): boolean {
  const present = existsSync(goldenPath);
  if (!present && process.env.CI) {
    throw new Error(
      `oracle golden missing in CI: ${goldenPath}\n` +
        "The differential check would have silently skipped. Goldens are " +
        "committed to the repo, so this is a bad path or a dropped file, " +
        "not an unfrozen dump.",
    );
  }
  return present;
}
