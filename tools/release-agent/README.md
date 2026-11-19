# release-agent

One container that turns merged PRs on `develop` into SemVer releases (#70).

```
PR into develop ──► check-pr: title lint (fails the check) + Claude review comment
merge to develop ─► on-push: release-please keeps "chore(release): vX.Y.Z" PR up to date,
                    CHANGELOG [Unreleased] stamped as vX.Y.Z (+ Claude highlights)
merge release PR ─► publish: tag vX.Y.Z (draft release) → libobd2.h asset built and compiled
   (or Monday,       → notes from CHANGELOG → main merged to the tag → release published
    ≥ N changes)     → issues closed by the released PRs get "Released in vX.Y.Z"
```

**Who decides what.** release-please computes the version from the squash-commit titles
(`fix` → patch, `feat` → minor, `!` → minor while on 0.x, major from 1.0). Claude only reads
and returns text through `.claude/skills/release/SKILL.md`; the script does every write.
Without an Anthropic credential the pipeline runs the same, minus the review and highlights.

## Run it anywhere

```sh
docker build -f tools/release-agent/Dockerfile -t libobd2-release-agent .

docker run --rm -e GH_TOKEN -e REPO=AndreiSaldorfean/libOBD2 \
  -e CLAUDE_CODE_OAUTH_TOKEN libobd2-release-agent status
```

| Command | Does |
| --- | --- |
| `check-pr <N>` | lint title, warn on missing changelog entry / `Closes #N`, post one sticky review comment |
| `on-push` | `publish`, then `release-pr` (run on every push to `develop`) |
| `release-pr` | open/update the release PR and stamp its CHANGELOG.md |
| `publish` | if the release PR was merged: create and finish the release |
| `weekly` | the Monday policy: if at least `RELEASE_MIN_CHANGES` (3) `feat`/`fix`/`perf`/breaking changes are unreleased **and** the release PR's CI is green, merge it and publish; otherwise do nothing |
| `finish <X.Y.Z>` | redo the finishing steps of a draft release after a failure |
| `status` | last release, release PR, what is unreleased |

Without Docker, from a clone: `GH_TOKEN=$(gh auth token) WORKSPACE=$PWD python3 tools/release-agent/release_agent.py status`
(`DRY_RUN=1` prints the `check-pr` comment instead of posting it; `CLAUDE_STEPS=force` uses your local `claude` login).

### Credentials

- `GH_TOKEN` (the `RELEASE_TOKEN` secret in Actions): a fine-grained PAT of a **repository admin** with
  Contents, Pull requests, Issues and **Workflows** read/write, and Commit statuses read.
  - *Admin*, because `develop` and `main` are protected (the `ci/*` checks from #69 are required) and
    `publish` pushes the release merge straight to `main` before CI has built it; admins bypass that rule.
  - *Workflows*, because a release that touches `.github/workflows/` is otherwise rejected on the `main`
    push and the release stays a draft.
  - `GITHUB_TOKEN` is enough for `check-pr` only: it cannot push to protected `main`, and PRs it opens
    do not trigger other workflows.
- `ANTHROPIC_API_KEY` or `CLAUDE_CODE_OAUTH_TOKEN` (from `claude setup-token`): optional.

### Where it runs

- **GitHub Actions**: `.github/workflows/release-agent.yml` builds the image and maps events to commands.
- **Jenkins**: `docker run ... libobd2-release-agent check-pr "$CHANGE_ID"` in a PR stage, `on-push` on `develop`.
- **Any box with cron**: `0 6 * * 1 docker run --rm --env-file /etc/release-agent.env libobd2-release-agent weekly`.

## CI gate

`weekly` never releases a broken `develop`. Before it merges the release PR it waits (up to
`RELEASE_CHECKS_TIMEOUT_MIN`, 30) for the commit statuses in `RELEASE_REQUIRED_CHECKS` (default
`ci/build-matrix,ci/unit-tests,ci/coverage`, the Jenkins checks of #69) to be green on the PR head,
and merges exactly that commit (`--match-head-commit`). A red or unfinished check skips the release
for the week and leaves a comment on the release PR. It does not re-push an up-to-date release PR
first, since that would restart its CI. Merging the release PR by hand goes through the same
branch protection. `status` shows the release PR's CI. Set `RELEASE_REQUIRED_CHECKS` to empty only
for a repository without CI.

## Repository settings this relies on

- Squash merging only, with **"Default to pull request title and description"** (Settings → General →
  Pull Requests), so the commit on `develop` is the linted PR title plus the PR body with its
  `Closes #N`. With "commit or PR title", one-commit PRs use the commit subject instead.
- Branch protection on `develop` and `main` requiring `ci/build-matrix`, `ci/unit-tests`, `ci/coverage`,
  without "include administrators" (see `GH_TOKEN`).
- `release-please-config.json`: `draft` + `force-tag-creation` keep a release private until its assets
  are built; `always-update` rebuilds the release PR on every push, so edit `[Unreleased]` on `develop`,
  not on the release branch.
- No `release-as`: release-please picks the next version from the commit types. One was only needed to
  force the first release (v0.1.0); a pinned version that is already released keeps the release PR on it
  (`status` warns).
