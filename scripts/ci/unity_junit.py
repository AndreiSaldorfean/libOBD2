#!/usr/bin/env python3
"""Turn a Unity test log into a JUnit report, and check the summary (#69).

    python3 scripts/ci/unity_junit.py builds/ci/unit_tests.txt builds/ci/unit_tests.xml [--max-ignored N]

Unity prints one line per test, `<file>:<line>:<test>:PASS|FAIL|IGNORE[:<message>]`, then
`<N> Tests <F> Failures <I> Ignored`. Exit code 1 when a test failed, when the summary line is
missing (the binary crashed or hung), or when more than --max-ignored tests were ignored.
"""
import argparse
import re
import sys
from pathlib import Path
from xml.sax.saxutils import escape

RESULT_RE = re.compile(r"^(?P<file>[^:\n]+):(?P<line>\d+):(?P<name>[^:\n]+):(?P<status>PASS|FAIL|IGNORE)(?::\s?(?P<msg>.*))?$")
SUMMARY_RE = re.compile(r"^(?P<tests>\d+) Tests (?P<failures>\d+) Failures (?P<ignored>\d+) Ignored")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("log", type=Path)
    parser.add_argument("junit", type=Path)
    parser.add_argument("--max-ignored", type=int, default=None, help="fail when more tests than this are ignored")
    args = parser.parse_args()

    cases, summary = [], None
    for line in args.log.read_text(errors="replace").splitlines():
        line = line.rstrip("\r")
        if m := RESULT_RE.match(line):
            cases.append(m.groupdict())
        elif m := SUMMARY_RE.match(line):
            summary = {k: int(v) for k, v in m.groupdict().items()}

    failed = [c for c in cases if c["status"] == "FAIL"]
    ignored = [c for c in cases if c["status"] == "IGNORE"]
    xml = ['<?xml version="1.0" encoding="UTF-8"?>',
           f'<testsuite name="unit-tests-host_virtual" tests="{len(cases)}" failures="{len(failed)}" skipped="{len(ignored)}">']
    for c in cases:
        cls = Path(c["file"]).stem
        xml.append(f'  <testcase classname="{escape(cls)}" name="{escape(c["name"])}">')
        where = f'{c["file"]}:{c["line"]}'
        if c["status"] == "FAIL":
            xml.append(f'    <failure message="{escape(c["msg"] or "failed", {chr(34): "&quot;"})}">{escape(where)}</failure>')
        elif c["status"] == "IGNORE":
            xml.append(f'    <skipped message="{escape(c["msg"] or "ignored", {chr(34): "&quot;"})}"/>')
        xml.append("  </testcase>")
    if summary is None:
        xml.append('  <testcase classname="unity" name="summary"><failure message="no Unity summary line: '
                   'the test binary crashed, hung or printed nothing"/></testcase>')
    xml.append("</testsuite>")
    args.junit.write_text("\n".join(xml) + "\n")

    if summary is None:
        print("FAIL: no Unity summary line; the test binary crashed, hung or printed nothing", file=sys.stderr)
        return 1
    print(f"{summary['tests']} tests, {summary['failures']} failures, {summary['ignored']} ignored")
    for c in failed:
        print(f"  FAIL {c['file']}:{c['line']} {c['name']}: {c['msg']}")
    rc = 1 if summary["failures"] else 0
    if args.max_ignored is not None and summary["ignored"] > args.max_ignored:
        print(f"FAIL: {summary['ignored']} ignored tests, baseline allows {args.max_ignored} "
              "(tests/ignored_baseline.txt). IGNORE is only for PBT_KNOWN_BUG properties with a bug ticket.")
        rc = 1
    elif args.max_ignored is not None and summary["ignored"] < args.max_ignored:
        print(f"Ignored tests went down to {summary['ignored']}: lower tests/ignored_baseline.txt in this PR.")
    return rc


if __name__ == "__main__":
    sys.exit(main())
