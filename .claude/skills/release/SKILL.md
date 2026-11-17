---
name: release
description: libOBD2 release conventions and the judgment half of the release pipeline. Modes - `status` (what is unreleased and what the next version is), `check-branch` (review local commits and a proposed PR title/body before the PR exists), `check-pr <N>` (review a PR's title, type and breaking changes), `notes <version>` (draft release highlights). Use when asked about releases, versions, changelog, PR titles, or when the release agent calls it.
---

# libOBD2 release skill

Releases are automated (#70). **release-please** decides the version, tag and GitHub
Release from conventional PR titles. `tools/release-agent/` runs it and does every write.
This skill supplies the judgment: is a PR breaking, is its type right, what are the
highlights. **In every mode you are read-only:** no commits, pushes, tags, comments,
labels or issue changes. When the release agent calls you, it posts your output itself.

## Conventions

PR titles, and so the squash-merge commits on `develop`, are `type(scope)!: summary`.

| Type | Bump while on 0.x | Bump from 1.0 |
| --- | --- | --- |
| `fix`, `perf` | patch | patch |
| `feat` | minor | minor |
| any type with `!`, or a `BREAKING CHANGE:` footer | **minor** (`bump-minor-pre-major`) | major |
| `refactor`, `test`, `docs`, `build`, `ci`, `chore`, `style`, `revert` | none | none |

Scopes in use: `app`, `datalink`, `iso9141`, `kwp2000`, `host_virtual`, `host_serial`,
`stm32f4`, `stm32g4`, `ecu`, `build`, `tests`, `ci`, `release`.

**The public contract.** A change is breaking when it changes any of these in a way that
makes existing user code or config stop compiling or behave differently:
- Headers in `include/` (`statusRetCodes.h`, `timing_if.h`, `uart_if.h`: return codes, `obd_uart_ops_t` and the timer ops a port implements).
- Public app headers in `src/app/`: `libobd2.h`, `libobd2_services.h`, `libobd2_service_requests.h`, `iso15031_5.h`, `libobd2_formulas.h`, `libobd2_transfer_protocol.h`.
- Config macros: `LIBOBD2_*`, `SPT_*`, `LOGGING`, `P*_TIME_*` (renamed, removed, or default changed).
- `build_config.json` keys/values, Makefile targets, CMake options, platform names, port directory layout.
- The single header `libobd2.h` (once #60 is merged): `LIBOBD2_IMPLEMENTATION` usage.

Changes to `tests/`, `ecu/`, `examples/` or private headers are not breaking.

**Changelog.** `CHANGELOG.md` is written by hand under `## [Unreleased]` (Keep a Changelog:
Added / Changed / Fixed / Removed, with `**Breaking:**` on breaking entries). `feat` and `fix` PRs
should add an entry. The release agent stamps `[Unreleased]` as the new version
(`scripts/changelog.py stamp`).

## Mode: `status`

Collect, with `git` and `gh` (read-only):
1. Last tag (`git describe --tags --abbrev=0 origin/develop`, or the latest GitHub release when the tag is not on `develop`) and the commits on `origin/develop` since it.
2. Each commit's type, and the next version by the table above. Note any `Release-As:` footer or `release-as` in `release-please-config.json`.
3. The open release PR (`gh pr list --base develop --label "autorelease: pending"`) and its age.
4. Commits whose subject is not conventional (they will not count toward a release).
5. Issues that the release will close (`Closes/Fixes/Resolves #N` in the PR bodies).

Answer in this format:

```
Next release: v<X.Y.Z> (<reason, e.g. "2 feat, 1 fix since v0.1.0">)
Release PR: #<N>, open <D> days | none yet
Will close: #a, #b
Not counted (non-conventional subjects): <short list or "none">
Unreleased changelog: <one line summary, or "empty - add entries">
```

When a person runs this mode by hand, end with at most three concrete next steps.

## Mode: `check-branch [<base>]`

Used by the `commit-changes` skill (step 6) before a PR is opened, or by hand. Input: the
current branch, and optionally a proposed PR title and body. `<base>` defaults to `origin/develop`.

1. `git fetch origin` is allowed (read-only for the remote). Then read
   `git log --format='%h %s%n%b' <base>..HEAD`, `git diff --stat <base>...HEAD` and
   `git diff <base>...HEAD`.
2. Apply the same checks as `check-pr` (type, breaking, changelog, linked issue), plus:
   - **Title:** matches `^(feat|fix|perf|refactor|test|docs|build|ci|chore|style|revert)(\([a-z0-9_./-]+\))?!?: \S`
     (the agent's regex); type is the highest-impact commit (breaking > feat > fix/perf > rest).
   - **Commits:** every subject on the branch is conventional; list any that are not.
   - **Body:** has `Closes #N` and that issue exists (`gh issue view N`); `BREAKING CHANGE:`
     appears only when the title has `!`; no other issue is named with closes/fixes/resolves by mistake.
   - **Branch:** not `develop` or `main`; its issue number (from `<issue>-<slug>`) matches `Closes #N`.
3. Output the same table as `check-pr`, with a `Commits` row added, then `Suggested title:` /
   `Suggested body change:` only when something should change. Do not push or open anything.

## Mode: `check-pr <N>`

1. Read the PR: `gh pr view <N> --json title,body,files,baseRefName,labels` and `gh pr diff <N>`.
2. If the PR has the `autorelease: pending` label, output only `Release PR, nothing to review.`
3. Decide:
   - **Type:** does the diff match the title type? For example, a "refactor" that changes behavior is a `fix` or `feat`, and a "fix" that adds API is a `feat`.
   - **Breaking:** does the diff touch the public contract above in an incompatible way? Quote the exact symbol, macro or file. If it is breaking and the title has no `!`, say so.
   - **Changelog:** for `feat`/`fix`/breaking changes, does the diff add an entry under `[Unreleased]`? Is a breaking entry marked `**Breaking:**`?
   - **Linked issue:** does the body contain `Closes #N`?
4. Output only the comment body in markdown, at most about 15 lines, in exactly this shape:

```
**Release check**: <✅ looks right | ⚠️ needs attention>

| | |
| --- | --- |
| Type | `<type>` <✅ / ⚠️ suggest `<type>`> |
| Breaking | <no / **yes**: `<symbol>` in `<file>`> |
| Bumps | <none / patch / minor / major> → next `vX.Y.Z` if merged now |
| Changelog | <✅ / ⚠️ missing entry under [Unreleased]> |
| Linked issue | <#N / ⚠️ none> |

Suggested title: `<title>` (only when it should change)

<one or two sentences explaining any ⚠️, nothing else>
```

## Mode: `notes <version>`

Input: the `[Unreleased]` section of `CHANGELOG.md` and the PRs since the last tag
(`git log <last-tag>..origin/develop --format='%s'`, `gh pr view <N> --json title,body`).
Output only markdown, starting with `### Highlights`:
- `### Highlights`: 2 to 4 bullets in plain words for someone who uses the library and has not read the PRs. Say what they can do now that they could not before.
- `### Upgrading`: only if something is breaking. Numbered steps that tell the reader exactly what to change, taken from the `**Breaking:**` entries.
- Do not repeat the full changelog: it follows your text in the release notes. No preamble, no closing remarks.
