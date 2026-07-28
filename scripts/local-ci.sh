#!/bin/sh
# local-ci.sh - run every CI leg that CAN run on this machine, then stamp the result.
#
# Why this exists: GitHub Actions minutes are a real budget. The branch model already
# keeps dev free (no workflow triggers on it), but the moment work is promoted to
# staging four workflows can fire, two of them on Windows runners. A leg that fails
# remotely for a reason reproducible locally is pure waste, so nothing gets promoted
# until it has been cleared here.
#
# On success this writes .git/local-ci-pass recording the exact commit that was
# cleared. .githooks/pre-push reads that stamp and refuses to push staging or
# production unless it matches the commit being pushed. See docs/BRANCHING.md.
#
# WHAT THIS COVERS, and what it honestly cannot:
#
#   native.yml      FULLY covered - native/verify.sh is literally the same script CI
#                   runs, on the same arm64 macOS toolchain.
#   port-tests.yml  FULLY covered - `pnpm test` in port/ is the same vitest run.
#   oracle.yml      NOT covered. Needs MSVC + a Windows runner to build the engine
#                   and capture the goldens. There is no local proxy; the native leg
#                   diffs against those goldens but cannot regenerate them.
#   build-modern    NOT covered, same reason.
#
# So a clean run here means "everything reproducible off-Windows passes". The two
# Windows legs remain a genuine remote risk, which is why the summary says so out
# loud rather than reporting an unqualified pass.

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

STAMP=.git/local-ci-pass
SHA=$(git rev-parse HEAD)
DIRTY=$(git status --porcelain)

pass=""
fail=""
skip=""

hr() { echo "------------------------------------------------------------"; }

run_leg() {
    name=$1
    shift
    hr
    echo "LEG: $name"
    hr
    if "$@"; then
        pass="$pass $name"
    else
        fail="$fail $name"
    fi
}

# --- leg: native macOS port (mirrors .github/workflows/native.yml) ---
# verify.sh is `set -e` and exits non-zero on any golden mismatch, so its exit
# status is the whole verdict.
run_leg native ./native/verify.sh

# --- leg: TypeScript port tests (mirrors .github/workflows/port-tests.yml) ---
if [ -d port ] && command -v pnpm > /dev/null 2>&1; then
    run_leg port sh -c 'cd port && pnpm test --run 2>&1 | tail -25'
else
    skip="$skip port(no-pnpm)"
fi

# --- the Windows legs ---
# Recorded as skipped rather than silently omitted: a clearance stamp that did not
# say which legs never ran would read as broader than it is.
skip="$skip oracle(needs-msvc) build-modern(needs-msvc)"

hr
echo "LOCAL CI SUMMARY"
hr
echo "commit    :$SHA"
[ -n "$pass" ] && echo "passed    :$pass"
[ -n "$fail" ] && echo "FAILED    :$fail"
[ -n "$skip" ] && echo "not local :$skip"

if [ -n "$fail" ]; then
    echo ""
    echo "Local CI failed. Nothing to promote - fix the above and re-run."
    rm -f "$STAMP"
    exit 1
fi

if [ -n "$DIRTY" ]; then
    echo ""
    echo "Working tree is dirty, so this run does not clear commit $SHA:"
    git status --short
    echo ""
    echo "The legs passed against the tree on disk, not against any commit. Commit the"
    echo "changes and re-run to earn a stamp."
    rm -f "$STAMP"
    exit 1
fi

printf '%s\n' "$SHA" > "$STAMP"
printf 'legs-passed:%s\n' "$pass" >> "$STAMP"
printf 'legs-not-runnable-locally:%s\n' "$skip" >> "$STAMP"

echo ""
echo "Cleared $SHA for promotion. The Windows legs ($skip) still run remotely."
