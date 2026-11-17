#!/usr/bin/env python3
"""Keep CHANGELOG.md (Keep a Changelog format) in step with releases.

  changelog.py stamp 0.1.0 [--date 2026-11-17] [--highlights FILE]
      Move the [Unreleased] entries under a new "## [0.1.0](...) - date"
      heading and leave an empty [Unreleased]. Does nothing when 0.1.0 is
      already stamped and [Unreleased] is empty, so it can be re-run.
  changelog.py notes 0.1.0
      Print the body of the 0.1.0 section (for the GitHub Release notes).
  changelog.py unreleased
      Print the body of [Unreleased]; exit 1 when it is empty.
"""

import argparse
import datetime
import re
import sys
from pathlib import Path

UNRELEASED_RE = re.compile(r"^## \[Unreleased\][^\n]*\n", re.MULTILINE)
SECTION_RE = re.compile(r"^## \[", re.MULTILINE)
TAG_LINK_RE = re.compile(r"\((https://github\.com/[^/\s]+/[^/\s]+)/releases/tag/v[^)\s]+\)")


def _section_bounds(text: str, heading_re: re.Pattern) -> tuple[int, int, int] | None:
    """Return (heading start, body start, body end) of the first match."""
    m = heading_re.search(text)
    if not m:
        return None
    nxt = SECTION_RE.search(text, m.end())
    return m.start(), m.end(), nxt.start() if nxt else len(text)


def _version_re(version: str) -> re.Pattern:
    return re.compile(rf"^## \[{re.escape(version)}\][^\n]*\n", re.MULTILINE)


def _has_entries(body: str) -> bool:
    """True when a section body holds more than headings and blank lines."""
    return any(line.strip() and not line.startswith("#") for line in body.splitlines())


def _repo_url(text: str, explicit: str | None) -> str | None:
    if explicit:
        return f"https://github.com/{explicit}"
    m = TAG_LINK_RE.search(text)
    return m.group(1) if m else None


def section_body(text: str, version: str) -> str | None:
    bounds = _section_bounds(text, UNRELEASED_RE if version == "Unreleased" else _version_re(version))
    if bounds is None:
        return None
    _, start, end = bounds
    return text[start:end].strip("\n")


def stamp(text: str, version: str, date: str, repo: str | None, highlights: str | None) -> str:
    bounds = _section_bounds(text, UNRELEASED_RE)
    if bounds is None:
        raise SystemExit("CHANGELOG.md has no '## [Unreleased]' section")
    head, start, end = bounds
    body = text[start:end].strip("\n")

    if _version_re(version).search(text):
        if _has_entries(body):
            raise SystemExit(f"{version} is already in CHANGELOG.md but [Unreleased] is not empty")
        return text  # already stamped

    if not _has_entries(body):
        body = "_No user-facing changes._"
    if highlights and highlights.strip():
        body = f"{highlights.strip()}\n\n{body}"

    url = _repo_url(text, repo)
    title = f"[{version}]({url}/releases/tag/v{version})" if url else f"[{version}]"
    new = f"## [Unreleased]\n\n## {title} - {date}\n\n{body}\n\n"
    return text[:head] + new + text[end:].lstrip("\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--file", type=Path, default=Path("CHANGELOG.md"))
    sub = parser.add_subparsers(dest="cmd", required=True)
    p_stamp = sub.add_parser("stamp")
    p_stamp.add_argument("version")
    p_stamp.add_argument("--date", default=datetime.date.today().isoformat())
    p_stamp.add_argument("--repo", help="owner/name, used for the heading link when the file has none yet")
    p_stamp.add_argument("--highlights", type=Path, help="markdown placed at the top of the new section")
    p_notes = sub.add_parser("notes")
    p_notes.add_argument("version")
    sub.add_parser("unreleased")
    args = parser.parse_args()

    text = args.file.read_text(encoding="utf-8")
    if args.cmd == "stamp":
        highlights = args.highlights.read_text(encoding="utf-8") if args.highlights else None
        new = stamp(text, args.version, args.date, args.repo, highlights)
        if new != text:
            args.file.write_text(new, encoding="utf-8")
            print(f"stamped {args.version}")
        else:
            print(f"{args.version} already stamped")
        return 0

    version = "Unreleased" if args.cmd == "unreleased" else args.version
    body = section_body(text, version)
    if body is None:
        print(f"no [{version}] section in {args.file}", file=sys.stderr)
        return 1
    print(body)
    return 0 if args.cmd != "unreleased" or _has_entries(body) else 1


if __name__ == "__main__":
    sys.exit(main())
