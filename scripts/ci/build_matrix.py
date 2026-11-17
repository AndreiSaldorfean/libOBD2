#!/usr/bin/env python3
"""Build every port with every app it supports (#69). Same command locally and in Jenkins.

    python3 scripts/ci/build_matrix.py                 # all cells
    python3 scripts/ci/build_matrix.py --list          # show the cells
    python3 scripts/ci/build_matrix.py stm32f4 host_   # only cells whose name contains a filter

Each cell is configured with scripts/gen.py (overrides, no edits to build_config.json) into its
own directory builds/ci/<cell>/ and built there; its full log is builds/ci/logs/<cell>.log.
A JUnit report (one test case per cell) is written to builds/ci/build_matrix.xml.
Exit code: 0 when every cell builds, 1 otherwise.

The STM32 cells need arm-none-eabi-gcc on PATH, or ARM_TOOLCHAIN_BIN pointing at its bin/.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "builds" / "ci"

# name: (app, platform, memory, type, extra --set options)
# app "single-header" is the #60 smoke test (make amalgamate_check): generate dist/libobd2.h and
# compile + link it with gcc and arm-none-eabi-gcc.
# Supported combinations per port:
#   host_virtual: library + unit tests (the tests are run by scripts/ci/unit_tests.sh)
#   host_serial:  library + obd2_reader example (no unit tests by design)
#   stm32f4:      library (FreeRTOS + tracing) + example + tests (flash, ram)
#   stm32g4:      library (tracing) + example + tests (flash)
CELLS = {
    "library-host-Debug":             ("library", "host_virtual", "flash", "Debug",   []),
    "library-host-Release":           ("library", "host_virtual", "flash", "Release", []),
    "host_virtual-tests-Debug":       ("tests",   "host_virtual", "flash", "Debug",   []),
    "host_virtual-tests-Release":     ("tests",   "host_virtual", "flash", "Release", []),
    "host_serial-example-Debug":      ("example", "host_serial",  "flash", "Debug",   []),
    "host_serial-example-Release":    ("example", "host_serial",  "flash", "Release", []),
    "stm32f4-library-rtos-trace":     ("library", "stm32f4",      "flash", "Debug",   ["SPT_FREERTOS=ON", "SPT_TRACING=ON"]),
    "stm32f4-example-flash-Debug":    ("example", "stm32f4",      "flash", "Debug",   []),
    "stm32f4-example-flash-Release":  ("example", "stm32f4",      "flash", "Release", []),
    "stm32f4-tests-flash-Debug":      ("tests",   "stm32f4",      "flash", "Debug",   []),
    "stm32f4-tests-ram-Debug":        ("tests",   "stm32f4",      "ram",   "Debug",   []),
    "stm32g4-library-trace":          ("library", "stm32g4",      "flash", "Debug",   ["SPT_TRACING=ON"]),
    "stm32g4-example-flash-Debug":    ("example", "stm32g4",      "flash", "Debug",   []),
    "stm32g4-tests-flash-Debug":      ("tests",   "stm32g4",      "flash", "Debug",   []),
    "single-header-check":            ("single-header", "-",          "-",     "-",       []),
}

# Cells that do not build yet, each with the issue that tracks it. They are still built on every run
# and reported as skipped; a failure here does not fail the matrix. When one builds again, the run
# says so: remove it from this list in the PR that fixed it.
KNOWN_BROKEN = {}

LIBOPENCM3 = ROOT / "examples" / "libs" / "libopencm3"
LIBOPENCM3_LIBS = {"stm32f4": "libopencm3_stm32f4.a", "stm32g4": "libopencm3_stm32g4.a"}


def app_dir(root: Path, app: str, memory: str, build_type: str) -> Path:
    """Where gen.py puts the app's build directory (see create_cfg in scripts/gen.py)."""
    return root / "library" if app == "library" else root / f"{app}_{memory}_{build_type}"


def run_logged(cmd: list[str], log, cwd: Path = ROOT) -> int:
    log.write(f"$ {' '.join(map(str, cmd))}\n")
    log.flush()
    return subprocess.run(cmd, cwd=cwd, stdout=log, stderr=subprocess.STDOUT).returncode


def ensure_libopencm3(platforms: set[str], jobs: int) -> str | None:
    """Build the libopencm3 archives the STM32 examples and tests link against. Returns an error or None."""
    missing = [p for p in sorted(platforms & LIBOPENCM3_LIBS.keys())
               if not (LIBOPENCM3 / "lib" / LIBOPENCM3_LIBS[p]).exists()]
    if not missing:
        return None
    targets = " ".join(f"stm32/{p[len('stm32'):]}" for p in missing)
    OUT.joinpath("logs").mkdir(parents=True, exist_ok=True)
    with open(OUT / "logs" / "libopencm3.log", "w") as log:
        rc = run_logged(["make", f"-j{jobs}", f"TARGETS={targets}"], log, cwd=LIBOPENCM3)
    return None if rc == 0 else f"libopencm3 ({targets}) failed, see builds/ci/logs/libopencm3.log"


def warnings_in(log_path: Path) -> int:
    text = log_path.read_text(errors="replace")
    return sum(1 for line in text.splitlines() if "warning:" in line)


def build_cell(name: str, jobs: int) -> tuple[bool, float, str]:
    app, platform, memory, build_type, sets = CELLS[name]
    root = OUT / name
    shutil.rmtree(root, ignore_errors=True)
    log_path = OUT / "logs" / f"{name}.log"
    start = time.monotonic()
    with open(log_path, "w") as log:
        if app == "single-header":
            ok = run_logged(["make", "amalgamate_check", f"amal_dir={root}"], log) == 0
            return ok, time.monotonic() - start, log_path.read_text(errors="replace")
        gen = [sys.executable, "scripts/gen.py", "--app", app, "--platform", platform, "--memory", memory,
               "--type", build_type, "--build-root", str(root)]
        for opt in sets:
            gen += ["--set", opt]
        ok = run_logged(gen, log) == 0
        ok = ok and run_logged(["cmake", "--build", str(app_dir(root, app, memory, build_type)), f"-j{jobs}"], log) == 0
    elapsed = time.monotonic() - start
    return ok, elapsed, log_path.read_text(errors="replace")


def junit(results: list[tuple[str, bool, float, str]], path: Path) -> None:
    failures = sum(1 for n, ok, _, _ in results if not ok and n not in KNOWN_BROKEN)
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             f'<testsuite name="build-matrix" tests="{len(results)}" failures="{failures}">']
    for name, ok, elapsed, log in results:
        lines.append(f'  <testcase classname="build-matrix" name="{escape(name)}" time="{elapsed:.1f}">')
        if not ok and name in KNOWN_BROKEN:
            lines.append(f'    <skipped message="known broken, {KNOWN_BROKEN[name]}"/>')
        elif not ok:
            tail = "\n".join(log.splitlines()[-80:])
            lines.append(f'    <failure message="build failed">{escape(tail)}</failure>')
        lines.append("  </testcase>")
    lines.append("</testsuite>")
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("filters", nargs="*", help="only cells whose name contains one of these")
    parser.add_argument("--list", action="store_true", help="list the cells and exit")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    args = parser.parse_args()

    names = [n for n in CELLS if not args.filters or any(f in n for f in args.filters)]
    if args.list:
        for n in names:
            app, platform, memory, build_type, sets = CELLS[n]
            print(f"{n:34} {app:8} {platform:13} {memory:5} {build_type:7} {' '.join(sets)}")
        return 0
    if not names:
        print("no cell matches", file=sys.stderr)
        return 2

    if os.environ.get("ARM_TOOLCHAIN_BIN"):
        os.environ["PATH"] = os.environ["ARM_TOOLCHAIN_BIN"] + os.pathsep + os.environ["PATH"]
    (OUT / "logs").mkdir(parents=True, exist_ok=True)

    results = []
    platforms = {CELLS[n][1] for n in names}
    needs_arm = platforms & LIBOPENCM3_LIBS.keys() or any(CELLS[n][0] == "single-header" for n in names)
    if needs_arm and not shutil.which("arm-none-eabi-gcc"):
        print("arm-none-eabi-gcc not found: put it on PATH or set ARM_TOOLCHAIN_BIN", file=sys.stderr)
        return 1
    error = ensure_libopencm3(platforms, args.jobs)
    if error:
        print(error, file=sys.stderr)
        return 1

    for name in names:
        print(f"--- {name}", flush=True)
        ok, elapsed, log = build_cell(name, args.jobs)
        results.append((name, ok, elapsed, log))
        if not ok and name not in KNOWN_BROKEN:
            print("\n".join(log.splitlines()[-40:]))

    junit(results, OUT / "build_matrix.xml")
    print(f"\n{'cell':34} {'result':18} {'time':>6} {'warnings':>8}")
    for name, ok, elapsed, _ in results:
        status = "ok" if ok else f"known broken {KNOWN_BROKEN[name]}" if name in KNOWN_BROKEN else "FAILED"
        print(f"{name:34} {status:18} {elapsed:5.0f}s {warnings_in(OUT / 'logs' / f'{name}.log'):8}")
    failed = [n for n, ok, _, _ in results if not ok and n not in KNOWN_BROKEN]
    known = [n for n, ok, _, _ in results if not ok and n in KNOWN_BROKEN]
    fixed = [n for n, ok, _, _ in results if ok and n in KNOWN_BROKEN]
    built = len(results) - len(failed) - len(known)
    print(f"\n{built}/{len(results)} cells built" + (f", {len(known)} known broken" if known else "")
          + (f"; FAILED: {', '.join(failed)}" if failed else ""))
    for n in fixed:
        print(f"note: {n} builds now: remove it from KNOWN_BROKEN in scripts/ci/build_matrix.py ({KNOWN_BROKEN[n]})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
