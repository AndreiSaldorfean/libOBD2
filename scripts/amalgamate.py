"""Generate the stb-style single header libobd2.h.

Follows the local include graph from the public headers (interface part) and
from the private headers and library sources (implementation part), and writes
every file once, in dependency order. Local `#include "..."` lines that point
at an amalgamated file are dropped; system includes and local includes that
are not part of the library (e.g. "trace.h", "FreeRTOS.h") stay.

Since every .c file ends up in one translation unit, the script fails when two
source files declare the same file-scope static symbol or the same `#define`.

Usage:
    python scripts/amalgamate.py -o builds/dist/libobd2.h [--version 0.0.0]
"""
import argparse
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent

# Directories searched for `#include "..."`, same as the OBD2 target's include dirs
INCLUDE_DIRS = [
    "include",
    "src/datalink/common",
    "src/datalink/specific/iso9141",
    "src/datalink/specific/kwp2000",
    "src/utils/common",
    "src/app",
]

# Interface part: always visible to the integrator
PUBLIC_HEADERS = [
    "include/statusRetCodes.h",
    "include/timing_if.h",
    "include/uart_if.h",
    "src/app/libobd2.h",
    "src/app/libobd2_services.h",
    "src/app/libobd2_service_requests.h",
    "src/app/iso15031_5.h",
    "src/app/libobd2_formulas.h",
    "src/app/libobd2_transfer_protocol.h",
]

# Implementation part: only with LIBOBD2_IMPLEMENTATION
PRIVATE_HEADERS = [
    "src/utils/common/utils.h",
    "src/datalink/common/datalink_utils.h",
    "src/datalink/specific/iso9141/iso9141_priv.h",
    "src/datalink/specific/iso9141/iso9141.h",
    "src/datalink/specific/kwp2000/kwp2000.h",
]

# Same directories the OBD2 target globs in CMakeLists.txt
SOURCE_DIRS = [
    "src/app",
    "src/datalink/common",
    "src/datalink/specific/iso9141",
    "src/datalink/specific/kwp2000",
    "src/utils/common",
]

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
DEFINE_RE = re.compile(r'^\s*#\s*define\s+(\w+)')
IF_RE = re.compile(r'^\s*#\s*if(?:def|ndef)?\b')
IF0_RE = re.compile(r'^\s*#\s*if\s+0\b')
ELSE_RE = re.compile(r'^\s*#\s*(?:else|elif)\b')
ENDIF_RE = re.compile(r'^\s*#\s*endif\b')
# File-scope `static` / `OBD2_STATIC` function or variable: name before ( [ = or ;
STATIC_RE = re.compile(r'^(?:static|OBD2_STATIC)\b[^(\[=;]*?\b(\w+)\s*[(\[=;]')

BANNER = """\
/*
 * libobd2.h - single-header build of libOBD2 v{version}
 * https://github.com/AndreiSaldorfean/libOBD2
 *
 * GENERATED FILE, DO NOT EDIT. Made by scripts/amalgamate.py from the sources
 * in src/ and include/. Edit those and run `cmake --build <dir> --target amalgamate`.
 *
 * ---------------------------------------------------------------------------
 * USAGE
 *
 *   In exactly ONE .c file, define LIBOBD2_IMPLEMENTATION before the include:
 *
 *       #define LIBOBD2_IMPLEMENTATION
 *       #include "libobd2.h"
 *
 *   Every other file only includes the header and gets the declarations.
 *
 *   The integrator provides the UART and timer ops (obd_uart_ops_t,
 *   obd_timing_ops_t) and hands them to the library through dataLink_if_t,
 *   same as with the CMake build. The ports/ directory is not included.
 *
 * ---------------------------------------------------------------------------
 * CONFIGURATION
 *
 *   Define these before including the header. Use the same set in every file
 *   that includes it (a common compiler flag such as -DLIBOBD2_SPT_ISO9141 is
 *   the easiest way).
 *
 *   LIBOBD2_SPT_ISO9141      ISO 9141-2 support. On by default when no
 *                            LIBOBD2_SPT_* protocol is defined.
 *   LIBOBD2_SPT_KWP2000      KWP2000 (ISO 14230) support. Not implemented yet.
 *   LIBOBD2_STRICT_TIMING    Use the P1..P4 timing limits from the standard.
 *                            By default the maximum limits are relaxed to 5 s.
 *   P1_TIME_MIN ... P4_TIME_MAX
 *                            Override a single timing limit, in ms.
 *   SPT_FREERTOS             Build for FreeRTOS. Needs "FreeRTOS.h",
 *                            "FreeRTOSConfig.h" and "task.h" on the include path.
 *   SPT_TRACING              Timing trace hooks. Needs a "trace.h" that provides
 *                            TRACE_TIMER_LOG and the TRACE_TAG_* values (see
 *                            ports/<platform>/trace.h).
 *   NDEBUG                   Compiles out the OBD2_DEBUG_ASSERT precondition
 *                            checks (standard assert).
 *   UNIT_TESTS               Gives the internal OBD2_STATIC functions external
 *                            linkage so unit tests can call them.
 *
 *   The CMake LOGGING option has no effect on the library sources.
 *
 * ---------------------------------------------------------------------------
 * LICENSE
 *
{license}
 */
"""


def rel(path: Path) -> str:
    return path.relative_to(PROJECT_ROOT).as_posix()


def resolve_include(name: str, including_file: Path) -> Path | None:
    """Resolves a quoted include like the compiler: own directory first, then INCLUDE_DIRS."""
    for directory in [including_file.parent] + [PROJECT_ROOT / d for d in INCLUDE_DIRS]:
        candidate = (directory / name).resolve()
        if candidate.is_file() and PROJECT_ROOT in candidate.parents:
            return candidate
    return None


def local_includes(path: Path) -> list[Path]:
    deps = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = INCLUDE_RE.match(line)
        if match:
            dep = resolve_include(match.group(1), path)
            if dep is not None:
                deps.append(dep)
    return deps


def topo_order(entries: list[Path], emitted: set[Path]) -> list[Path]:
    """Depth-first post-order: every file comes after the files it includes."""
    order: list[Path] = []
    visiting: set[Path] = set()

    def visit(path: Path) -> None:
        if path in emitted or path in visiting:
            # In `visiting`: include cycle, the include guards resolve it like the compiler does
            return
        visiting.add(path)
        for dep in local_includes(path):
            visit(dep)
        visiting.discard(path)
        emitted.add(path)
        order.append(path)

    for entry in entries:
        visit(entry)
    return order


def strip_if0(lines: list[str]) -> list[str]:
    """Drops `#if 0 ... #endif` blocks. A block with an #else / #elif branch is kept as it is."""
    out: list[str] = []
    idx = 0
    while idx < len(lines):
        if not IF0_RE.match(lines[idx]):
            out.append(lines[idx])
            idx += 1
            continue

        depth = 0
        has_else = False
        end = idx
        for end in range(idx, len(lines)):
            if IF_RE.match(lines[end]):
                depth += 1
            elif ENDIF_RE.match(lines[end]):
                depth -= 1
                if depth == 0:
                    break
            elif depth == 1 and ELSE_RE.match(lines[end]):
                has_else = True
        else:
            sys.exit(f"amalgamate: unterminated #if 0 at line {idx + 1}")

        if has_else:
            out.extend(lines[idx:end + 1])
        idx = end + 1
    return out


def file_body(path: Path, amalgamated: set[Path]) -> str:
    lines = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = INCLUDE_RE.match(line)
        if match:
            dep = resolve_include(match.group(1), path)
            if dep is not None and dep in amalgamated:
                continue
        lines.append(line.rstrip())
    lines = strip_if0(lines)

    while lines and not lines[-1]:
        lines.pop()
    header = f"/* {'=' * 20} {rel(path)} {'=' * max(1, 76 - len(rel(path)))} */"
    return header + "\n" + "\n".join(lines) + "\n"


def check_single_tu(sources: list[Path], headers: list[Path]) -> None:
    """Fails on file-scope statics or #defines that clash once all sources share one TU."""
    owners: dict[tuple[str, str], set[str]] = {}

    for path in sources + headers:
        statics: set[str] = set()
        defines: set[str] = set()
        for line in strip_if0(path.read_text(encoding="utf-8").splitlines()):
            if path.suffix == ".c":
                match = STATIC_RE.match(line)
                if match:
                    statics.add(match.group(1))
            match = DEFINE_RE.match(line)
            if match:
                defines.add(match.group(1))
        for name in statics:
            owners.setdefault(("static", name), set()).add(rel(path))
        for name in defines:
            owners.setdefault(("#define", name), set()).add(rel(path))

    clashes = []
    for (kind, name), files in sorted(owners.items()):
        sources_involved = [f for f in files if f.endswith(".c")]
        # A header #define seen twice is harmless (guards); a .c one clashing with anything is not
        if len(files) > 1 and sources_involved:
            clashes.append(f"  {kind} {name}: {', '.join(sorted(files))}")
    if clashes:
        sys.exit("amalgamate: names clash in the single translation unit:\n" + "\n".join(clashes))


def license_text() -> str:
    lines = (PROJECT_ROOT / "LICENSE").read_text(encoding="utf-8").strip().splitlines()
    return "\n".join((" * " + line).rstrip() for line in lines)


def amalgamate(version: str) -> str:
    sources = sorted(
        path.resolve()
        for directory in SOURCE_DIRS
        for path in (PROJECT_ROOT / directory).glob("*.c")
    )
    public = [PROJECT_ROOT / p for p in PUBLIC_HEADERS]
    private = [PROJECT_ROOT / p for p in PRIVATE_HEADERS]
    for path in public + private:
        if not path.is_file():
            sys.exit(f"amalgamate: missing {rel(path)}")

    emitted: set[Path] = set()
    interface = topo_order(public, emitted)
    impl_headers = topo_order(private, emitted)
    # Headers the sources pull in that the lists above miss
    impl_headers += [p for p in topo_order(sources, emitted) if p.suffix == ".h"]

    amalgamated = set(interface) | set(impl_headers) | set(sources)
    check_single_tu(sources, interface + impl_headers)

    out = [BANNER.format(version=version, license=license_text())]
    out.append("#ifndef LIBOBD2_SINGLE_HEADER_H")
    out.append("#define LIBOBD2_SINGLE_HEADER_H\n")
    out.append(f"#define LIBOBD2_VERSION \"{version}\"\n")
    out += [file_body(p, amalgamated) for p in interface]
    out.append("#endif /* LIBOBD2_SINGLE_HEADER_H */\n")

    out.append("/* " + "=" * 37 + " IMPLEMENTATION " + "=" * 37 + " */")
    out.append("#ifdef LIBOBD2_IMPLEMENTATION")
    out.append("#ifndef LIBOBD2_IMPLEMENTATION_INCLUDED")
    out.append("#define LIBOBD2_IMPLEMENTATION_INCLUDED\n")
    out += [file_body(p, amalgamated) for p in impl_headers]
    out += [file_body(p, amalgamated) for p in sources]
    out.append("#endif /* LIBOBD2_IMPLEMENTATION_INCLUDED */")
    out.append("#endif /* LIBOBD2_IMPLEMENTATION */")
    return "\n".join(out) + "\n"


def parse_arguments():
    parser = argparse.ArgumentParser(description="Generate the stb-style single header libobd2.h")
    parser.add_argument("-o", "--output", type=Path, required=True, help="path of the generated header")
    parser.add_argument("--version", default="0.0.0", help="version written in the banner")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_arguments()
    text = amalgamate(args.version)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Always written: CMake reruns this when an input is newer than the output
    args.output.write_text(text, encoding="utf-8")
    print(f"amalgamate: wrote {args.output}")
