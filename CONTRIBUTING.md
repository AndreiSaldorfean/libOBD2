# Contributing

## Workflow

1. Open an issue. Branch from `develop` as `<issue>-<slug>` (GitHub's "Create a branch" does this).
2. Open a PR into `develop`:
   - **Title:** `type(scope): summary`, e.g. `feat(host_serial): fast-init wake-up` or `fix(iso9141)!: reject frames over 7 data bytes`.
   - **Description:** include `Closes #<issue>`. The issue is closed when the change is *released*.
   - **Changelog:** for `feat`/`fix`, add an entry under `## [Unreleased]` in `CHANGELOG.md`; mark API breaks with `**Breaking:**`.
3. Squash-merge. The release agent comments on every PR; a non-conventional title fails its check.

With Claude Code, `/commit-changes` does step 2 for you: it splits local work into
conventional commits (with their changelog entries), then proposes the PR title, body and
`Closes #N`, pre-checks them with the `release` skill (`check-branch`), and opens the PR
into `develop` once you approve.

| Type | Next version (0.x) | Use for |
| --- | --- | --- |
| `fix`, `perf` | patch | bug fixes, speedups |
| `feat` | minor | new capability |
| `…!` | minor (major from 1.0) | anything that breaks user code or config: see the public contract in `.claude/skills/release/SKILL.md` |
| `refactor`, `test`, `docs`, `build`, `ci`, `chore` | none | everything else |

## Releases

Nobody tags by hand. A bot PR `chore(release): vX.Y.Z` stays open on `develop`; merging it
(by hand at any time, or by the Monday job once at least `RELEASE_MIN_CHANGES`, default 3,
`feat`/`fix`/`perf`/breaking changes are waiting and its CI checks are green) tags the release, publishes notes and `libobd2.h`,
and moves `main` to it. Details: `tools/release-agent/README.md`.
