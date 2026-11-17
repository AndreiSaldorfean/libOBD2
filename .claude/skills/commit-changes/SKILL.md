---
name: commit-changes
description: Review the uncommitted changes in this repo, group them into logical conventional commits that the release agent accepts (feat, fix, perf, refactor, test, docs, build, ci, chore, style, revert), show the plan for approval, create the commits, and optionally open the PR into develop with a release-ready title and description. Use when the user asks to commit, split, or organize local changes, or to turn local work into a PR.
---

# Commit local changes

Turn the current working tree into a short series of clean, logical commits, and
optionally into a PR that the release agent (#70, `tools/release-agent/`) can release
without edits. Each commit holds one complete change, with all its files and hunks.

**How releases see your work.** PRs into `develop` are squash-merged with the **PR title**
as the commit subject and the **PR body** as the commit body. release-please computes the
next version only from that squash commit. Your branch commits are for review and
history; the PR title and body are what ship. The rules (types, scopes, the public
contract, what counts as breaking) live in `.claude/skills/release/SKILL.md`; read its
**Conventions** section before classifying.

## 1. Collect the changes

Run these and read the output in full:

```bash
git status --porcelain=v1 -uall
git diff --stat
git diff                    # unstaged
git diff --cached           # already staged
git log --oneline -15       # recent history, for context
```

- For untracked files, read them (`cat`, or `head` for large ones) so you know what they are.
- If `git status` shows a deleted file next to a new file with similar content, check it with `git diff --no-index` or by comparing the two. If it is a move, commit the delete and the add together so git can see the rename.
- If something is already staged, include it in the analysis. Don't assume the staging was intentional; ask if it doesn't fit the plan.

## 2. Understand each change

For each hunk, work out **why** it was made, not only what changed. Read the code around it when the diff alone isn't enough. Classify each hunk:

Use only these types. The release agent fails any PR whose title uses another one.

| Type       | Use for | Release effect (0.x) |
|------------|---------|----------------------|
| `feat`     | New functionality visible to a user of the library or app, including a new component or port | minor |
| `fix`      | A bug fix: corrects wrong behaviour (wrong value, crash, timing error, off-by-one, bad init, etc.) | patch |
| `perf`     | Makes something faster or use less memory, same behaviour | patch |
| `refactor` | Restructures code without changing behaviour (moves, renames, splits, removing duplicates) | none |
| `test`     | Adding, changing or reorganizing tests and test vectors | none |
| `docs`     | Documentation and comments only | none |
| `build`    | Build system: CMake, Makefile, build scripts, build config, toolchain files; third-party libraries and submodules as `build(deps)` | none |
| `ci`       | Jenkins, GitHub Actions, the release agent's wiring | none |
| `chore`    | Maintenance outside product code: .gitignore, editor config, cleanup, project scaffolding | none |
| `style`    | Formatting, whitespace, typos in strings and messages | none |
| `revert`   | Reverting an earlier commit | none |

**Breaking changes.** If a hunk changes the public contract (see the release skill:
`include/` headers, public `src/app/` headers, config macros, `build_config.json`, Makefile
targets, CMake options, platform names) so that existing user code or config stops
compiling or behaves differently, add `!` before the colon (`feat(app)!: ...`) and a
`BREAKING CHANGE: <what to change>` footer. Never write `BREAKING CHANGE:` otherwise:
release-please reads it.

**Changelog.** A `feat`, `fix`, `perf` or breaking commit includes its own entry under
`## [Unreleased]` in `CHANGELOG.md` (Added / Changed / Fixed / Removed; prefix breaking
entries with `**Breaking:**`). Write the entry for a library user, not about the diff. If
the change has no entry yet, draft one and put it in the plan for approval; the CHANGELOG
hunk goes in the same commit as the change it describes.

## 3. Group into commits

Rules, in order of priority:

1. **Bug fixes get their own commit.** If a hunk fixes a bug, it goes in a separate `fix` commit, even if it sits in a file that also has a refactor or feature. Split the file by hunk (see step 5). Include the hunks needed to make the fix complete (e.g. the fix plus the test that covers it, or the fix in the source plus its header).
2. **Group by intent, not by file.** One commit is one logical change, with every file and hunk it touches: source, header, CMake entry, test and config. Do not make one commit per file.
   - Good: `refactor(ecu): move UART driver into shared port layer` with the deleted `ecu_ports/*/ecu_uart.*`, the new files, and the `CMakeLists.txt` hunks that point to them.
   - Bad: three commits, `delete ecu_uart.c`, `add new uart file`, `update CMakeLists.txt`.
3. **Every commit should build.** Order commits so that dependencies come first (e.g. add a vendored library before the code that uses it; add a new API before the callers).
4. **Do not over-split.** Small related tweaks that serve the same goal belong together. Aim for the fewest commits that still keep each one focused.
5. **Do not mix unrelated changes** just to reduce the number of commits.

### Files to leave out

Do not commit generated output or local-only files, for example:
- simulation/trace dumps (`*.vcd`), build directories, binaries, logs
- personal editor settings (`.vscode/`) unless the repo already tracks shared ones

List them in the plan under **Not committed**, and suggest a `.gitignore` entry. Do not change `.gitignore` unless the user approves it.

Also flag anything that looks unfinished or accidental: debug prints, commented-out blocks, `TODO` left from this change, folders like `*_old/`. Ask what to do with it instead of guessing.

## 4. Show the plan and wait

Before any `git add` or `git commit`, present the plan:

```
Commit 1/N  fix(datalink): correct checksum on ISO9141 response
  ecu/modules/datalink/specific/l2_iso9141/ecu_l2_iso9141.c  (hunk @@ -120,7 lines: checksum loop)
  CHANGELOG.md  (new entry under [Unreleased] / Fixed, drafted below)
  Why: loop skipped the last byte, so valid frames were rejected.

Commit 2/N  refactor(ecu): move UART driver into shared port layer
  D ecu/ecu_ports/stm32f4/ecu_uart.c
  ...

Not committed:
  test_*.vcd  - generated traces; suggest adding `*.vcd` to .gitignore

Questions:
  - tests/src/unit_old/ looks like a leftover copy. Commit, delete, or ignore?

PR (step 6, if wanted):
  fix(datalink): correct checksum on ISO9141 response   -> v0.3.1 when released
  Closes #62 (from branch 62-iso9141-checksum)
```

For each commit, give the message, the files (and which hunks when a file is split), and a one-line reason. Then **stop and wait for the user's approval**. Apply any changes they ask for and show the updated plan if they change much.

## 5. Create the commits

Make sure nothing is staged except what you plan for the current commit (`git diff --cached --stat`). Then for each commit:

- Whole files: `git add -- <paths>` (this also stages deletions). For moves, `git add` both the old and new paths.
- Part of a file: interactive `git add -p` is not available. Instead write a patch with only the wanted hunks and stage it:
  ```bash
  git diff -U3 -- <file> > "$SCRATCH/full.patch"
  # copy the header plus the wanted hunks into $SCRATCH/part.patch
  git apply --cached --check "$SCRATCH/part.patch" && git apply --cached "$SCRATCH/part.patch"
  ```
  Use the session scratchpad directory for the patch files. If two needed hunks are too close to separate, use `-U0` with `git apply --cached --unidiff-zero`, or ask the user.
- Check: `git diff --cached --stat` (and `git diff --cached` for split files) matches the plan.
- Commit with a heredoc so the message keeps its formatting.

After the last commit, run `git status` and `git log --oneline -N` and show the result. Anything left uncommitted should be only the files listed under **Not committed**.

## 6. Open the PR (when the user asks, or approves it in the plan)

1. **Check the branch, right before pushing.** `git branch --show-current` and
   `gh pr list --head <branch> --state all`. Never push to `develop` or `main`. If the work
   sits on `develop`/`main`, ask for an issue number, then `git switch -c <issue>-<slug>`
   at the current commit. Don't reset the local `develop`/`main`; tell the user it still
   has the commits. If the branch already has a merged PR, stop and ask.
2. **Issue.** Take the number from the branch name (`<issue>-<slug>`). If there is none, ask.
   Read the issue (`gh api repos/{owner}/{repo}/issues/<N> --jq .title,.body`) so the
   description matches it.
3. **Title** = the squash commit subject, so it decides the version:
   - The type is the highest-impact commit on the branch (`git log origin/develop..HEAD`):
     breaking > `feat` > `fix`/`perf` > the rest. Add `!` if any commit is breaking.
   - Scope: the common scope of the commits that drive that type; leave it out if they span areas.
   - Subject: what the PR delivers as a whole, imperative, lower case, about 72 characters total.
   - If the branch mixes unrelated user-visible changes (say a `feat` in `host_serial` and an
     unrelated `fix` in `iso9141`), suggest splitting the PR: only one title reaches the changelog
     line release-please computes. The hand-written CHANGELOG still lists both.
4. **Body** = the squash commit body:
   ```
   <2-4 sentences: what this changes for a library user and why>

   ## Changes
   - <one line per commit subject, or per logical group>

   ## Testing
   - <what was run: `make test`, coverage, hardware, or "not run" with the reason>

   Closes #<issue>

   BREAKING CHANGE: <exact upgrade step>   <- only for `!` PRs
   ```
   Follow any PR attribution rules in effect for the session. Do not mention other issue
   numbers with "closes/fixes/resolves" unless this PR really finishes them: the release
   agent closes every issue referenced that way.
5. **Pre-check.** Run the release skill in `check-branch` mode on the proposed title and body.
   Fix anything it flags (wrong type, missing `!`, missing changelog entry), or show the flag to
   the user and ask.
6. **Show the PR and wait.** Show title, body, base `develop`, and the release effect
   (e.g. "minor: next release v0.2.0"). Push and open it only after approval:
   ```bash
   git push -u origin <branch>
   gh pr create --base develop --head <branch> --title "<title>" --body-file "$SCRATCH/pr.md"
   ```
   To change the title or body of an existing PR later, use
   `gh api -X PATCH repos/{owner}/{repo}/pulls/<N> -f title=... -F body=@file` (`gh pr edit`
   fails on this repo).
7. Report the PR URL. The `release-agent` check comments on the PR in CI; a failing title check
   blocks it. Nothing else is needed for the release: after the squash merge, the release
   agent adds the change to the `chore(release): vX.Y.Z` PR.

## Commit message format

```
<type>(<scope>): <subject>

<body: what changed and why, wrapped at 72 columns>
```

- **Subject**: imperative mood, lower case after the colon, no trailing period, at most ~72 characters in total. `fix(datalink): reject frames with bad checksum`, not `Fixed checksum.`
- **Scope**: optional, lower case. Use it when the change is inside one area of the repo: `app`, `datalink`, `iso9141`, `kwp2000`, `host_virtual`, `host_serial`, `stm32f4`, `stm32g4`, `ecu`, `build`, `tests`, `ci`, `release`, `doc`, `deps`. Leave it out when the change spans many areas.
- **Breaking**: `!` before the colon, plus a `BREAKING CHANGE:` footer (see step 2).
- **No issue or ticket number** in the message.
- **Body**: add one when the reason is not obvious from the subject. Always add one for `fix` commits: describe the wrong behaviour and how the change fixes it.
- Follow any commit attribution rules in effect for the session (e.g. a `Co-Authored-By` trailer).

## Don't

- Don't commit without the user's approval of the plan.
- Don't push, amend, rebase, or reset unless the user asks. Opening the PR (step 6) counts as asked only once the user approves the PR.
- Don't use types outside the table in step 2 (no `init`, `vendor`, `minor`).
- Don't use `git add -A` or `git add .`; always add explicit paths.
- Don't change code while committing. If you find a bug during the review, report it; don't fix it in these commits.
