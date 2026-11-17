#!/usr/bin/env python3
"""release-agent: SemVer releases for libOBD2 from conventional PR titles.

Deterministic tools do the writes (release-please, git, gh). Claude, when a credential
is present, only reads and returns text (PR review, release highlights) through the
`release` skill in .claude/skills/release/SKILL.md.

Commands:
  check-pr <N>     lint the PR title (fails when not conventional), post the review comment
  release-pr       open/update the release PR on develop and stamp its CHANGELOG.md
  publish          tag + finish the release once the release PR is merged
  on-push          publish, then release-pr (what a push to develop runs)
  weekly           merge the release PR when enough has landed (RELEASE_MIN_CHANGES) and its CI is green
  finish <X.Y.Z>   re-run the finishing steps of a draft release (assets, notes, main, issues)
  status           what is unreleased and what the next version is
  shell            open a shell in the prepared repository

Environment:
  GH_TOKEN                   required; contents, pull-requests and issues write
  REPO                       owner/name (default: GITHUB_REPOSITORY or the workspace remote)
  DEVELOP_BRANCH, MAIN_BRANCH  default develop / main
  RELEASE_MIN_CHANGES        default 3 (weekly): feat + fix + perf + breaking changes since the last release
  RELEASE_MIN_AGE_DAYS       default 0 (weekly): also require the release PR to be this many days old
  RELEASE_REQUIRED_CHECKS    default ci/build-matrix,ci/unit-tests,ci/coverage (weekly): commit statuses that
                             must be green on the release PR before it is merged; empty disables the gate
  RELEASE_CHECKS_TIMEOUT_MIN default 30 (weekly): how long to wait for pending checks
  ANTHROPIC_API_KEY or CLAUDE_CODE_OAUTH_TOKEN  optional; without them Claude steps are skipped
  CLAUDE_STEPS=off|force     skip Claude steps, or run them on an existing `claude` login (local use)
  CLAUDE_MODEL, CLAUDE_MAX_TURNS  optional
  WORKSPACE                  default /workspace; used when it holds a clone, else the repo is cloned
  DRY_RUN=1                  check-pr prints its comment instead of posting it
"""

import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
CHANGELOG_PY = HERE / "changelog.py"
if not CHANGELOG_PY.exists():  # running from the source tree, not the image
    CHANGELOG_PY = HERE.parents[1] / "scripts" / "changelog.py"

TYPES = "feat|fix|perf|refactor|test|docs|build|ci|chore|style|revert"
TITLE_RE = re.compile(rf"^(?P<type>{TYPES})(?:\((?P<scope>[a-z0-9_./-]+)\))?(?P<bang>!)?: \S")
CLOSE_RE = re.compile(r"\b(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?)\s*:?\s+#(\d+)", re.IGNORECASE)
PR_REF_RE = re.compile(r"\(#(\d+)\)\s*$")
MARKER = "<!-- release-agent -->"
RELEASE_LABEL = "autorelease: pending"
CLAUDE_READ_TOOLS = [
    "Read", "Grep", "Glob",
    "Bash(git log:*)", "Bash(git diff:*)", "Bash(git show:*)", "Bash(git describe:*)",
    "Bash(gh pr view:*)", "Bash(gh pr diff:*)", "Bash(gh pr list:*)",
    "Bash(gh issue view:*)", "Bash(gh release list:*)", "Bash(gh release view:*)",
]


def log(msg: str) -> None:
    print(f"[release-agent] {msg}", file=sys.stderr, flush=True)


def run(cmd: list[str], cwd: Path | None = None, check: bool = True, capture: bool = True) -> str:
    res = subprocess.run(cmd, cwd=cwd, text=True, capture_output=capture)
    if check and res.returncode != 0:
        detail = (res.stderr or res.stdout or "").strip() if capture else ""
        raise RuntimeError(f"{' '.join(cmd[:3])}... failed ({res.returncode}): {detail}")
    return (res.stdout or "").strip() if capture else ""


def ok(cmd: list[str], cwd: Path | None = None) -> bool:
    return subprocess.run(cmd, cwd=cwd, capture_output=True).returncode == 0


class Agent:
    def __init__(self) -> None:
        if not os.environ.get("GH_TOKEN"):
            raise SystemExit("GH_TOKEN is required")
        self.token = os.environ["GH_TOKEN"]
        self.develop = os.environ.get("DEVELOP_BRANCH", "develop")
        self.main = os.environ.get("MAIN_BRANCH", "main")
        self.tmp = Path(tempfile.mkdtemp(prefix="release-agent-"))
        self.dir = self._prepare_repo()
        self.repo = os.environ.get("REPO") or os.environ.get("GITHUB_REPOSITORY") or self._repo_from_remote()

    # ---- setup -------------------------------------------------------------------------

    def _prepare_repo(self) -> Path:
        run(["gh", "auth", "setup-git"])
        ws = Path(os.environ.get("WORKSPACE", "/workspace"))
        if (ws / ".git").exists():
            repo_dir = ws
        else:
            name = os.environ.get("REPO") or os.environ.get("GITHUB_REPOSITORY")
            if not name:
                raise SystemExit("no clone in WORKSPACE and no REPO/GITHUB_REPOSITORY to clone")
            repo_dir = self.tmp / "repo"
            log(f"cloning {name}")
            run(["git", "clone", "--quiet", f"https://github.com/{name}.git", str(repo_dir)])
        run(["git", "fetch", "--quiet", "--prune", "--tags", "--force", "origin"], cwd=repo_dir)
        for key, default in (("user.name", "github-actions[bot]"),
                             ("user.email", "41898282+github-actions[bot]@users.noreply.github.com")):
            if not ok(["git", "config", key], cwd=repo_dir):
                run(["git", "config", key, os.environ.get(f"GIT_{key.split('.')[1].upper()}", default)], cwd=repo_dir)
        return repo_dir

    def _repo_from_remote(self) -> str:
        url = run(["git", "remote", "get-url", "origin"], cwd=self.dir)
        m = re.search(r"github\.com[:/]([^/]+/[^/]+?)(?:\.git)?$", url)
        if not m:
            raise SystemExit(f"cannot tell owner/name from remote {url}; set REPO")
        return m.group(1)

    def worktree(self, ref: str, name: str) -> Path:
        """A throwaway checkout of ref, so a mounted workspace is never switched."""
        path = self.tmp / name
        run(["git", "worktree", "add", "--quiet", "--detach", str(path), ref], cwd=self.dir)
        return path

    # ---- GitHub helpers ----------------------------------------------------------------

    def gh(self, *args: str, check: bool = True) -> str:
        return run(["gh", *args], cwd=self.dir, check=check)

    def gh_json(self, *args: str):
        out = self.gh(*args)
        return json.loads(out) if out else None

    def release_pr(self) -> dict | None:
        prs = self.gh_json("pr", "list", "--repo", self.repo, "--base", self.develop, "--state", "open",
                           "--label", RELEASE_LABEL, "--json", "number,headRefName,title,createdAt,url")
        return prs[0] if prs else None

    def release(self, tag: str) -> dict | None:
        res = subprocess.run(["gh", "release", "view", tag, "--repo", self.repo, "--json",
                              "tagName,isDraft,url,body"], cwd=self.dir, text=True, capture_output=True)
        return json.loads(res.stdout) if res.returncode == 0 else None

    def upsert_comment(self, number: int, body: str) -> None:
        if os.environ.get("DRY_RUN"):
            print(f"--- comment for #{number} (DRY_RUN, not posted) ---\n{body}")
            return
        comments = self.gh_json("api", "--paginate", "--slurp", f"repos/{self.repo}/issues/{number}/comments")
        mine = [c for page in comments for c in page if MARKER in (c.get("body") or "")]
        if mine:
            self.gh("api", "-X", "PATCH", f"repos/{self.repo}/issues/comments/{mine[0]['id']}", "-f", f"body={body}")
        else:
            self.gh("api", "-X", "POST", f"repos/{self.repo}/issues/{number}/comments", "-f", f"body={body}")

    def release_please(self, command: str) -> None:
        run(["release-please", command, "--token", self.token, "--repo-url", self.repo,
             "--target-branch", self.develop, "--config-file", "release-please-config.json",
             "--manifest-file", ".release-please-manifest.json"], cwd=self.dir, capture=False)

    # ---- Claude (read-only, optional) --------------------------------------------------

    def claude(self, prompt: str, cwd: Path) -> str | None:
        has_cred = os.environ.get("ANTHROPIC_API_KEY") or os.environ.get("CLAUDE_CODE_OAUTH_TOKEN")
        mode = os.environ.get("CLAUDE_STEPS", "on")  # on | off | force (use an existing local login)
        if mode == "off" or not (has_cred or mode == "force") or not shutil.which("claude"):
            log("Claude step skipped (no credential, no CLI, or CLAUDE_STEPS=off)")
            return None
        cmd = ["claude", "-p", prompt, "--output-format", "text",
               "--max-turns", os.environ.get("CLAUDE_MAX_TURNS", "25"),
               "--disallowedTools", "Write,Edit,NotebookEdit,WebFetch,WebSearch"]
        if os.environ.get("CLAUDE_MODEL"):
            cmd += ["--model", os.environ["CLAUDE_MODEL"]]
        cmd += ["--allowedTools", *CLAUDE_READ_TOOLS]
        try:
            res = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True, timeout=900)
        except subprocess.TimeoutExpired:
            log("Claude step timed out; continuing without it")
            return None
        if res.returncode != 0 or not res.stdout.strip():
            log(f"Claude step failed; continuing without it: {res.stderr.strip()[:300]}")
            return None
        return res.stdout.strip()

    # ---- unreleased changes -------------------------------------------------------------

    def last_release_tag(self) -> str | None:
        return run(["git", "describe", "--tags", "--abbrev=0", "--match", "v[0-9]*", f"origin/{self.develop}"],
                   cwd=self.dir, check=False) or self.previous_tag("")

    def unreleased(self) -> tuple[str | None, dict[str, int]]:
        """Commits on develop since the last release, by kind. `releasable` is what the weekly policy counts."""
        last = self.last_release_tag()
        rng = f"{last}..origin/{self.develop}" if last else f"origin/{self.develop}"
        log_out = run(["git", "log", "--format=%s%x00%b%x1e", rng], cwd=self.dir)
        kinds = {"breaking": 0, "feat": 0, "fix": 0, "perf": 0, "other": 0, "not conventional": 0}
        for entry in filter(None, (e.strip() for e in log_out.split("\x1e"))):
            subject, _, body = entry.partition("\x00")
            if subject.startswith("chore(release):"):
                continue
            m = TITLE_RE.match(subject)
            if not m:
                kinds["not conventional"] += 1
            elif m["bang"] or re.search(r"^BREAKING[ -]CHANGE:", body, re.MULTILINE):
                kinds["breaking"] += 1
            elif m["type"] in ("feat", "fix", "perf"):
                kinds[m["type"]] += 1
            else:
                kinds["other"] += 1
        kinds["releasable"] = kinds["breaking"] + kinds["feat"] + kinds["fix"] + kinds["perf"]
        return last, kinds

    # ---- CI gate ------------------------------------------------------------------------

    @staticmethod
    def required_checks() -> list[str]:
        raw = os.environ.get("RELEASE_REQUIRED_CHECKS", "ci/build-matrix,ci/unit-tests,ci/coverage")
        return [c.strip() for c in raw.split(",") if c.strip()]

    def checks_state(self, sha: str, required: list[str]) -> tuple[str, dict[str, str]]:
        """success when every required status is success on sha; failure when one failed; else pending."""
        data = self.gh_json("api", f"repos/{self.repo}/commits/{sha}/status") or {}
        latest = {s["context"]: s["state"] for s in data.get("statuses", [])}  # newest first per context
        states = {c: latest.get(c, "missing") for c in required}
        if any(v in ("failure", "error") for v in states.values()):
            return "failure", states
        if all(v == "success" for v in states.values()):
            return "success", states
        return "pending", states

    def wait_for_checks(self, sha: str) -> tuple[str, dict[str, str]]:
        required = self.required_checks()
        if not required:
            log("RELEASE_REQUIRED_CHECKS is empty: releasing without a CI gate")
            return "success", {}
        timeout = float(os.environ.get("RELEASE_CHECKS_TIMEOUT_MIN", "30")) * 60
        deadline = time.monotonic() + timeout
        while True:
            state, states = self.checks_state(sha, required)
            if state != "pending" or time.monotonic() >= deadline:
                return state, states
            log(f"waiting for CI on {sha[:7]}: " + ", ".join(f"{c} {v}" for c, v in states.items()))
            time.sleep(60)

    def release_pr_is_current(self, pr: dict) -> bool:
        """The release PR already contains develop and its stamped changelog: nothing to re-push."""
        head = pr["headRefName"]
        run(["git", "fetch", "--quiet", "origin", f"+refs/heads/{head}:refs/remotes/origin/{head}"], cwd=self.dir)
        if not ok(["git", "merge-base", "--is-ancestor", f"origin/{self.develop}", f"origin/{head}"], cwd=self.dir):
            return False
        manifest = run(["git", "show", f"origin/{head}:.release-please-manifest.json"], cwd=self.dir, check=False)
        changelog = run(["git", "show", f"origin/{head}:CHANGELOG.md"], cwd=self.dir, check=False)
        version = json.loads(manifest or "{}").get(".")
        return bool(version) and re.search(rf"^## \[{re.escape(version)}\]", changelog, re.MULTILINE) is not None

    @staticmethod
    def min_changes() -> int:
        return max(1, int(os.environ.get("RELEASE_MIN_CHANGES", "3")))

    # ---- commands ----------------------------------------------------------------------

    def check_pr(self, number: int) -> int:
        pr = self.gh_json("pr", "view", str(number), "--repo", self.repo,
                          "--json", "title,body,labels,files")
        if RELEASE_LABEL in [label["name"] for label in pr["labels"]]:
            log(f"#{number} is the release PR; nothing to check")
            return 0
        title, body = pr["title"], pr.get("body") or ""
        m = TITLE_RE.match(title)
        lines = [MARKER]
        if m:
            lines.append(f"**Title**: ✅ `{title}`")
        else:
            lines.append(f"**Title**: ❌ `{title}` is not `type(scope): summary`. Types: `{TYPES.replace('|', '`, `')}`; "
                         "add `!` before `:` for a breaking change. The title becomes the squash commit, "
                         "which decides the next version.")
        if m and (m["type"] in ("feat", "fix", "perf") or m["bang"]):
            if not any(f["path"] == "CHANGELOG.md" for f in pr["files"]):
                lines.append("**Changelog**: ⚠️ no entry under `[Unreleased]` in CHANGELOG.md")
        if not CLOSE_RE.search(body):
            lines.append("**Issue**: ⚠️ no `Closes #N` in the description (the issue would not be closed on release)")

        review = self.claude(
            f"Use the `release` skill (.claude/skills/release/SKILL.md) in `check-pr {number}` mode "
            f"for pull request #{number} of {self.repo}. Print only the comment body.", cwd=self.dir)
        if review:
            lines += ["", review]
        self.upsert_comment(number, "\n".join(lines))
        log(f"#{number}: title {'ok' if m else 'NOT conventional'}")
        return 0 if m else 1

    def release_pr_cmd(self) -> int:
        self.release_please("release-pr")
        pr = self.release_pr()
        # Right after a merge, GitHub's API can lag behind git for a few seconds, and release-please then
        # sees no new commits. When the clone shows releasable changes, give the API time and retry.
        for attempt in range(3):
            if pr or self.unreleased()[1]["releasable"] == 0:
                break
            log(f"git shows releasable changes but release-please opened no PR; retrying in 30 s ({attempt + 1}/3)")
            time.sleep(30)
            run(["git", "fetch", "--quiet", "--prune", "--tags", "--force", "origin"], cwd=self.dir)
            self.release_please("release-pr")
            pr = self.release_pr()
        if not pr:
            log("no releasable changes; no release PR")
            return 0
        head = pr["headRefName"]
        run(["git", "fetch", "--quiet", "origin", f"+refs/heads/{head}:refs/remotes/origin/{head}"], cwd=self.dir)
        wt = self.worktree(f"origin/{head}", "release-pr")
        version = json.loads((wt / ".release-please-manifest.json").read_text())["."]
        changelog = (wt / "CHANGELOG.md").read_text(encoding="utf-8")
        if re.search(rf"^## \[{re.escape(version)}\]", changelog, re.MULTILINE):
            log(f"release PR #{pr['number']} already has v{version} stamped")
            return 0

        stamp = [sys.executable, str(CHANGELOG_PY), "stamp", version, "--repo", self.repo]
        notes = self.claude(
            f"Use the `release` skill (.claude/skills/release/SKILL.md) in `notes {version}` mode. "
            f"The repository is {self.repo}; compare against the latest tag reachable from origin/{self.develop} "
            f"or the latest GitHub release. Print only the markdown.", cwd=wt)
        if notes and notes.lstrip().startswith("### Highlights"):
            hl = self.tmp / "highlights.md"
            hl.write_text(notes + "\n", encoding="utf-8")
            stamp += ["--highlights", str(hl)]
        elif notes:
            log("Claude notes did not start with '### Highlights'; left out")
        run(stamp, cwd=wt)
        if ok(["git", "diff", "--quiet"], cwd=wt):
            return 0
        run(["git", "commit", "--quiet", "-am", f"chore(release): stamp CHANGELOG.md for v{version}"], cwd=wt)
        run(["git", "push", "--quiet", "origin", f"HEAD:refs/heads/{head}"], cwd=wt)
        log(f"release PR #{pr['number']} ({pr['url']}) is ready for v{version}")
        return 0

    def publish(self) -> int:
        self.release_please("github-release")
        run(["git", "fetch", "--quiet", "--tags", "--force", "origin", self.develop], cwd=self.dir)
        manifest = run(["git", "show", f"origin/{self.develop}:.release-please-manifest.json"], cwd=self.dir)
        version = json.loads(manifest)["."]
        rel = self.release(f"v{version}")
        if not rel or not rel["isDraft"]:
            log(f"nothing to finish (v{version} {'already published' if rel else 'not released'})")
            return 0
        return self.finish(version)

    def finish(self, version: str) -> int:
        tag = f"v{version}"
        rel = self.release(tag)
        if not rel:
            raise SystemExit(f"no GitHub release {tag}")
        run(["git", "fetch", "--quiet", "--tags", "--force", "origin", self.main], cwd=self.dir)
        wt = self.worktree(tag, "release")
        prev = self.previous_tag(tag)

        # 1. assets: fail here and the release stays a draft
        assets = self.build_assets(wt, version)
        if assets:
            self.gh("release", "upload", tag, "--repo", self.repo, "--clobber", *map(str, assets))

        # 2. notes from the stamped changelog section
        body = run([sys.executable, str(CHANGELOG_PY), "--file", str(wt / "CHANGELOG.md"), "notes", version],
                   check=False) or rel.get("body") or ""
        if prev:
            body += f"\n\n**Full changelog**: https://github.com/{self.repo}/compare/{prev}...{tag}"
        notes = self.tmp / "notes.md"
        notes.write_text(body + "\n", encoding="utf-8")
        self.gh("release", "edit", tag, "--repo", self.repo, "--title", f"libOBD2 {tag}", "--notes-file", str(notes))

        # 3. main follows the release
        self.sync_main(tag)

        # 4. go public, then close what shipped
        self.gh("release", "edit", tag, "--repo", self.repo, "--draft=false", "--latest")
        log(f"published https://github.com/{self.repo}/releases/tag/{tag}")
        self.close_released_issues(prev, tag, f"https://github.com/{self.repo}/releases/tag/{tag}")
        return 0

    def previous_tag(self, tag: str) -> str | None:
        prev = run(["git", "describe", "--tags", "--abbrev=0", f"{tag}^"], cwd=self.dir, check=False)
        if prev:
            return prev
        rels = self.gh_json("release", "list", "--repo", self.repo, "--limit", "20",
                            "--json", "tagName,isDraft,publishedAt") or []
        older = [r["tagName"] for r in rels if not r["isDraft"] and r["tagName"] != tag]
        return older[0] if older else None

    def build_assets(self, wt: Path, version: str) -> list[Path]:
        script = wt / "scripts" / "amalgamate.py"
        if not script.exists():
            log("no scripts/amalgamate.py at this tag; no single-header asset")
            return []
        out = self.tmp / "dist"
        out.mkdir(exist_ok=True)
        header = out / "libobd2.h"
        run([sys.executable, str(script), "-o", str(header), "--version", version], cwd=wt)
        (out / "impl.c").write_text('#define LIBOBD2_IMPLEMENTATION\n#include "libobd2.h"\n')
        (out / "user.c").write_text('#include "libobd2.h"\nint main(void) { return 0; }\n')
        for src in ("impl.c", "user.c"):  # the same flags as the #60 smoke test
            run(["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(out), "-c", str(out / src),
                 "-o", str(out / (src + ".o"))])
        log("libobd2.h generated and compiles")
        return [header]

    def sync_main(self, tag: str) -> None:
        if ok(["git", "merge-base", "--is-ancestor", tag, f"origin/{self.main}"], cwd=self.dir):
            log(f"{self.main} already contains {tag}")
            return
        wt = self.worktree(f"origin/{self.main}", "main")
        res = subprocess.run(["git", "merge", "--no-edit", "-m", f"Merge release {tag} into {self.main}", tag],
                             cwd=wt, text=True, capture_output=True)
        if res.returncode != 0:
            subprocess.run(["git", "merge", "--abort"], cwd=wt)
            raise SystemExit(f"merging {tag} into {self.main} conflicts; release left as a draft.\n{res.stdout}")
        run(["git", "push", "--quiet", "origin", f"HEAD:refs/heads/{self.main}"], cwd=wt)
        log(f"{self.main} now contains {tag}")

    def close_released_issues(self, prev: str | None, tag: str, url: str) -> None:
        rng = f"{prev}..{tag}" if prev else tag
        subjects = run(["git", "log", "--format=%s", rng], cwd=self.dir).splitlines()
        prs = sorted({int(m.group(1)) for s in subjects if (m := PR_REF_RE.search(s))})
        issues: set[int] = set()
        for n in prs:
            res = subprocess.run(["gh", "pr", "view", str(n), "--repo", self.repo, "--json",
                                  "body,closingIssuesReferences"], cwd=self.dir, text=True, capture_output=True)
            if res.returncode != 0:
                continue  # an issue number, not a PR
            data = json.loads(res.stdout)
            issues |= {int(x) for x in CLOSE_RE.findall(data.get("body") or "")}
            issues |= {i["number"] for i in data.get("closingIssuesReferences") or []}
        # GitHub usually closed these already: sync_main put their "Closes #N" commits on main, the
        # default branch. Comment on every released issue anyway (once per tag), and close what is open.
        marker = f"<!-- release-agent:released {tag} -->"
        for n in sorted(issues - set(prs)):
            res = subprocess.run(["gh", "issue", "view", str(n), "--repo", self.repo, "--json", "state,comments"],
                                 cwd=self.dir, text=True, capture_output=True)
            if res.returncode != 0:
                continue
            data = json.loads(res.stdout)
            if not any(marker in (c.get("body") or "") for c in data.get("comments") or []):
                self.gh("issue", "comment", str(n), "--repo", self.repo, "--body", f"Released in [{tag}]({url}).\n{marker}")
                log(f"#{n}: commented 'Released in {tag}'")
            if data["state"] == "OPEN":
                self.gh("issue", "close", str(n), "--repo", self.repo, "--reason", "completed")
                log(f"closed #{n}")

    def weekly(self) -> int:
        """The Monday policy: release when at least RELEASE_MIN_CHANGES feat/fix/perf/breaking changes are unreleased."""
        last, kinds = self.unreleased()
        need = self.min_changes()
        if kinds["releasable"] < need:
            log(f"{kinds['releasable']}/{need} releasable changes since {last or 'the start'}; no release this week")
            return 0
        pr = self.release_pr()
        if not pr:
            self.release_pr_cmd()  # on-push should have opened it; open it now
            pr = self.release_pr()
            if not pr:
                log("no release PR to merge")
                return 0
        min_age = float(os.environ.get("RELEASE_MIN_AGE_DAYS", "0"))
        opened = datetime.datetime.fromisoformat(pr["createdAt"].replace("Z", "+00:00"))
        age = (datetime.datetime.now(datetime.timezone.utc) - opened).total_seconds() / 86400
        if age < min_age:
            log(f"release PR #{pr['number']} is {age:.1f} days old (< {min_age}); waiting")
            return 0
        log(f"{kinds['releasable']}/{need} releasable changes since {last or 'the start'}")
        if not self.release_pr_is_current(pr):
            # Re-pushing resets its CI, so only when develop moved on since on-push last ran.
            self.release_pr_cmd()
            pr = self.release_pr() or pr
        sha = self.gh_json("pr", "view", str(pr["number"]), "--repo", self.repo, "--json", "headRefOid")["headRefOid"]
        state, states = self.wait_for_checks(sha)
        summary = ", ".join(f"`{c}` {v}" for c, v in states.items())
        if state != "success":
            verdict = "failed" if state == "failure" else "did not finish in time"
            log(f"CI {verdict} on release PR #{pr['number']} ({summary}); no release this week")
            self.upsert_comment(pr["number"], f"{MARKER}\n**Monday release skipped**: CI {verdict} on "
                                f"`{sha[:7]}` ({summary}). Fix `develop`, or merge this PR by hand once it is green.")
            return 1 if state == "failure" else 0
        # --match-head-commit: never merge a commit other than the one that passed CI
        self.gh("pr", "merge", str(pr["number"]), "--repo", self.repo, "--squash", "--match-head-commit", sha)
        log(f"merged release PR #{pr['number']} at {sha[:7]} ({summary or 'no CI gate'})")
        return self.publish()

    def status(self) -> int:
        manifest = json.loads(run(["git", "show", f"origin/{self.develop}:.release-please-manifest.json"],
                                  cwd=self.dir, check=False) or '{".": "?"}')
        pr = self.release_pr()
        print(f"Last released (manifest): v{manifest['.']}")
        print(f"Release PR: {('#%d %s' % (pr['number'], pr['title'])) if pr else 'none'}")
        if pr and self.required_checks():
            sha = self.gh_json("pr", "view", str(pr["number"]), "--repo", self.repo, "--json", "headRefOid")["headRefOid"]
            state, states = self.checks_state(sha, self.required_checks())
            print(f"Release PR CI: {state} (" + ", ".join(f"{c} {v}" for c, v in states.items()) + ")")
        last, kinds = self.unreleased()
        counts = ", ".join(f"{v} {k}" for k, v in kinds.items() if v and k != "releasable")
        print(f"Since {last or 'the start'}: {counts or 'nothing'}")
        need = self.min_changes()
        verdict = "releases" if kinds["releasable"] >= need else "no release"
        print(f"Monday policy: {kinds['releasable']}/{need} feat/fix/perf/breaking changes -> {verdict}")
        cfg = json.loads((self.dir / "release-please-config.json").read_text()) \
            if (self.dir / "release-please-config.json").exists() else {}
        if cfg.get("release-as") and cfg["release-as"] == manifest.get("."):
            print(f"⚠️  release-as {cfg['release-as']} is already released: remove it from release-please-config.json")
        summary = self.claude(f"Use the `release` skill in `status` mode for {self.repo}.", cwd=self.dir)
        if summary:
            print("\n" + summary)
        return 0


def main(argv: list[str]) -> int:
    if not argv or argv[0] in ("-h", "--help", "help"):
        print(__doc__)
        return 0
    cmd, args = argv[0], argv[1:]
    agent = Agent()
    try:
        if cmd == "check-pr" and len(args) == 1:
            return agent.check_pr(int(args[0]))
        if cmd == "release-pr":
            return agent.release_pr_cmd()
        if cmd == "publish":
            return agent.publish()
        if cmd == "on-push":
            return agent.publish() or agent.release_pr_cmd()
        if cmd == "weekly":
            return agent.weekly()
        if cmd == "finish" and len(args) == 1:
            return agent.finish(args[0].lstrip("v"))
        if cmd == "status":
            return agent.status()
        if cmd == "shell":
            return subprocess.call([os.environ.get("SHELL", "bash")], cwd=agent.dir)
        print(__doc__)
        return 2
    finally:
        for wt in agent.tmp.glob("*"):
            if wt.name != "repo" and (wt / ".git").is_file():
                subprocess.run(["git", "worktree", "remove", "--force", str(wt)], cwd=agent.dir, capture_output=True)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
