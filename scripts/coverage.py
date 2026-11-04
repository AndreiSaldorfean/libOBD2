"""Line / branch / function coverage of the library code, from a gcov-instrumented test build.

Used by `make cc`:
    python scripts/coverage.py --reset builds/tests_flash_Debug_cc   # drop counts from earlier runs
    ./builds/tests_flash_Debug_cc/test                                # run the tests (writes .gcda)
    python scripts/coverage.py builds/tests_flash_Debug_cc            # print the report

Only files under the --include paths are reported (default: the library sources and the host
port), minus anything matching --exclude (default: third-party code and the unfinished KWP2000
protocol). Needs gcc >= 9 for `gcov --json-format`.
"""
import argparse
import collections
import glob
import gzip
import json
import os
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def parse_arguments():
    parser = argparse.ArgumentParser(description="Report gcov coverage of the libOBD2 sources")
    parser.add_argument("build_dir", help="coverage build directory, e.g. builds/tests_flash_Debug_cc")
    parser.add_argument("--reset", action="store_true", help="delete the .gcda counters and exit")
    parser.add_argument("--uncovered", action="store_true", help="also list uncovered lines and never-called functions")
    parser.add_argument("--include", nargs="+", default=["src", "ports/host"],
                        help="paths (relative to the repo) to report on (default: src ports/host)")
    parser.add_argument("--exclude", nargs="+", default=["third_party", "kwp2000"],
                        help="path parts to leave out (default: third_party kwp2000)")
    return parser.parse_args()


def find_gcda(build_dir: Path) -> list[Path]:
    return [Path(p) for p in glob.glob(str(build_dir / "**" / "*.gcda"), recursive=True)]


def run_gcov(build_dir: Path, gcda_files: list[Path]) -> Path:
    out_dir = build_dir / "coverage"
    out_dir.mkdir(exist_ok=True)
    for old in out_dir.glob("*.gcov.json.gz"):
        old.unlink()

    for gcda in gcda_files:
        subprocess.run(["gcov", "--json-format", "--branch-probabilities", "--object-directory", str(gcda.parent), str(gcda)],
                       cwd=out_dir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    return out_dir


def collect(out_dir: Path, include: list[str], exclude: list[str]) -> dict:
    """Merge the gcov JSON of every object into per-source-file counters."""
    roots = [str(PROJECT_ROOT / p) + os.sep for p in include]
    files = collections.defaultdict(lambda: {"lines": {}, "branches": {}, "funcs": {}})

    for json_file in out_dir.glob("*.gcov.json.gz"):
        with gzip.open(json_file) as f:
            data = json.load(f)
        cwd = data.get("current_working_directory", "")

        for src in data["files"]:
            path = os.path.normpath(os.path.join(cwd, src["file"]))
            if not path.startswith(tuple(roots)) or any(part in path for part in exclude):
                continue
            rec = files[os.path.relpath(path, PROJECT_ROOT)]

            # A header's inline functions appear in every object that includes it: add the counts up
            for line in src["lines"]:
                num = line["line_number"]
                rec["lines"][num] = rec["lines"].get(num, 0) + line["count"]
                for idx, branch in enumerate(line.get("branches", [])):
                    rec["branches"][(num, idx)] = rec["branches"].get((num, idx), 0) + branch["count"]
            for func in src.get("functions", []):
                name = func.get("demangled_name", func["name"])
                count, start = rec["funcs"].get(name, (0, func["start_line"]))
                rec["funcs"][name] = (count + func["execution_count"], start)

    return files


def percent(hit: int, total: int) -> str:
    return f"{100 * hit / total:5.1f}%" if total else "    - "


def report(files: dict, uncovered: bool) -> None:
    print(f"{'file':50} {'lines':>16} {'branches':>16} {'functions':>10}")
    totals = [0, 0, 0, 0]

    for name in sorted(files):
        rec = files[name]
        lines, line_hit = len(rec["lines"]), sum(1 for c in rec["lines"].values() if c)
        branches, branch_hit = len(rec["branches"]), sum(1 for c in rec["branches"].values() if c)
        funcs, func_hit = len(rec["funcs"]), sum(1 for c, _ in rec["funcs"].values() if c)
        totals = [totals[0] + line_hit, totals[1] + lines, totals[2] + branch_hit, totals[3] + branches]

        print(f"{name:50} {line_hit:4}/{lines:<4} {percent(line_hit, lines)} "
              f"{branch_hit:4}/{branches:<4} {percent(branch_hit, branches)} {func_hit:4}/{funcs:<4}")

        if uncovered:
            missed = sorted(n for n, c in rec["lines"].items() if not c)
            never = [n for _, n in sorted((s, n) for n, (c, s) in rec["funcs"].items() if not c)]
            if missed:
                print(f"    uncovered lines: {missed}")
            if never:
                print(f"    never called:    {', '.join(never)}")

    print(f"{'TOTAL':50} {totals[0]:4}/{totals[1]:<4} {percent(totals[0], totals[1])} "
          f"{totals[2]:4}/{totals[3]:<4} {percent(totals[2], totals[3])}")


if __name__ == "__main__":
    args = parse_arguments()
    build_dir = Path(args.build_dir).resolve()
    gcda_files = find_gcda(build_dir)

    if args.reset:
        for gcda in gcda_files:
            gcda.unlink()
        sys.exit(0)

    if not gcda_files:
        print(f"Error: no .gcda files in {build_dir}; build with COVERAGE=ON and run the tests first", file=sys.stderr)
        sys.exit(1)

    report(collect(run_gcov(build_dir, gcda_files), args.include, args.exclude), args.uncovered)
