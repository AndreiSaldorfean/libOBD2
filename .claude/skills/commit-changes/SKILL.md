---
name: commit-changes
description: Review the uncommitted changes in this repo, work out what they do, group them into logical commits with prefixed messages (feat, fix, refactor, build, test...), show the plan for approval, then create the commits. Use when the user asks to commit, split, or organize local changes.
---

# Commit local changes

Turn the current working tree into a short series of clean, logical commits.
Each commit holds one complete change, with all its files and hunks.

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

| Prefix     | Use for |
|------------|---------|
| `init`     | Starting a project or a new component from scratch |
| `feat`     | New functionality visible to a user of the library or app |
| `fix`      | A bug fix: corrects wrong behaviour (wrong value, crash, timing error, off-by-one, bad init, etc.) |
| `refactor` | Restructures code without changing behaviour (moves, renames, splits, removing duplicates) |
| `perf`     | Makes something faster or use less memory, same behaviour |
| `build`    | Build system: CMake, Makefile, build scripts, build config, toolchain files |
| `chore`    | Maintenance outside product code: .gitignore, editor config, CI, cleanup |
| `vendor`   | Adding or updating third-party libraries (e.g. under `libs/`) |
| `test`     | Adding, changing or reorganizing tests and test vectors |
| `docs`     | Documentation and comments only |
| `style`    | Formatting and whitespace only |
| `minor`    | Very small changes with negligible impact (message text, typos in strings) |

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
  Why: loop skipped the last byte, so valid frames were rejected.

Commit 2/N  refactor(ecu): move UART driver into shared port layer
  D ecu/ecu_ports/stm32f4/ecu_uart.c
  ...

Not committed:
  test_*.vcd  - generated traces; suggest adding `*.vcd` to .gitignore

Questions:
  - tests/src/unit_old/ looks like a leftover copy. Commit, delete, or ignore?
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

## Commit message format

```
<type>(<scope>): <subject>

<body: what changed and why, wrapped at 72 columns>
```

- **Subject**: imperative mood, lower case after the colon, no trailing period, at most ~72 characters in total. `fix(datalink): reject frames with bad checksum`, not `Fixed checksum.`
- **Scope**: optional. Use it when the change is inside one area of the repo, e.g. `app`, `datalink`, `iso9141`, `kwp2000`, `ecu`, `ports`, `stm32f4`, `host`, `tests`, `build`. Leave it out when the change spans many areas.
- **No issue or ticket number** in the message.
- **Body**: add one when the reason is not obvious from the subject. Always add one for `fix` commits: describe the wrong behaviour and how the change fixes it.
- Follow any commit attribution rules in effect for the session (e.g. a `Co-Authored-By` trailer).

## Don't

- Don't commit without the user's approval of the plan.
- Don't push, amend, rebase, or reset unless the user asks.
- Don't use `git add -A` or `git add .`; always add explicit paths.
- Don't change code while committing. If you find a bug during the review, report it; don't fix it in these commits.
