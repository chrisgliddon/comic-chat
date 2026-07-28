# Working rules for this repo

## Branches and CI budget — read docs/BRANCHING.md

**Work on `dev`. Only ever on `dev`.** It is the default branch and has no workflow
triggers, so pushing it is free. `staging` and `production` are the only branches that
run CI, and two of the four workflows use Windows runners, which bill at a multiplier.
GitHub Actions minutes are a real constraint here.

**Never push `staging` or `production` without running the local legs first:**

```sh
./scripts/local-ci.sh
```

On success that stamps the cleared commit and `.githooks/pre-push` lets the promotion
through; without a matching stamp it refuses. Activate the hook once per clone with
`git config core.hooksPath .githooks`.

Do not trigger workflows remotely (`workflow_dispatch`, or pushing a CI branch) to find
out whether something works. Reproduce it locally. `native.yml` and `port-tests.yml` are
fully reproducible on this machine; `oracle.yml` and `build-modern.yml` need MSVC and are
the only legitimate reasons to spend a remote run.

## Test oracle

`docs/porting/RULEBOOK.md` holds the frozen porting facts and `docs/porting/LEDGER.md`
the run history. RULEBOOK 5 is load-bearing: text measurement comes from the frozen
glyph table (`oracle/glyphs/glyphs.json`) and never from live font measurement.

Prefer fixing a golden mismatch on the *read* side over re-freezing the golden — a
re-freeze costs a Windows CI run to regenerate data the repo already holds.
