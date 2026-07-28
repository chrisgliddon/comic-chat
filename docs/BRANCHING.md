# Branch model and CI budget

GitHub Actions minutes are a real constraint on this repo: two of the four workflows
run on Windows runners, which bill at a multiplier. The branch model exists to make
day-to-day work cost nothing and to make promotion a deliberate, locally-verified act.

## The three branches

| branch | role | CI |
|---|---|---|
| `dev` | **default branch. All work happens here.** | none — absent from every trigger |
| `staging` | promotion target; where CI actually runs | all four workflows |
| `production` | released state | all four workflows |

`dev` is not merely "untriggered by accident" — every workflow's `on.push.branches`
lists `staging` and `production` explicitly and omits `dev`. Pushing `dev` is free, so
push it as often as you like.

Nothing keys off `pull_request`: this repo promotes by pushing a branch, not by
opening PRs.

## Promotion is gated locally first

Never push `staging` or `production` before running:

```sh
./scripts/local-ci.sh
```

That runs every CI leg reproducible on this machine and, on success, writes
`.git/local-ci-pass` recording the cleared commit. `.githooks/pre-push` reads that
stamp and **refuses** to push `staging`/`production` unless it matches the commit
being pushed.

Activate the hook once per clone:

```sh
git config core.hooksPath .githooks
```

### What local clearance does and does not prove

| workflow | locally runnable? | why |
|---|---|---|
| `native.yml` | **yes, fully** | `native/verify.sh` is the same script CI runs, on the same arm64 macOS toolchain |
| `port-tests.yml` | **yes, fully** | `pnpm test` in `port/` is the same vitest run |
| `oracle.yml` | **no** | needs MSVC on a Windows runner to build the engine and capture goldens |
| `build-modern.yml` | **no** | same |

So a clean local run means *everything reproducible off-Windows passes*. The two
Windows legs stay a genuine remote risk. `local-ci.sh` names them as skipped in its
summary and in the stamp rather than reporting an unqualified pass — a stamp that
didn't say what never ran would read as broader than it is.

A dirty working tree never earns a stamp: the legs would have passed against the tree
on disk, not against any commit, and the hook compares against a commit.

Bypass, for when you know why you want it:

```sh
LOCAL_CI_OVERRIDE=1 git push origin staging
```

## Typical cycle

```sh
git checkout dev
# ...work...
git commit -am "..."
git push origin dev            # free, no CI

./scripts/local-ci.sh          # earn the stamp
git checkout staging
git merge --ff-only dev
git push origin staging        # hook verifies, CI runs
git checkout dev
```
