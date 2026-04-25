#!/usr/bin/env python3
"""
Build directory generator for libOBD2 project.
Generates CMake build directories with helper scripts for loading, running, and debugging firmware.
"""

import argparse
import os
import stat
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent

GDB_SERVER = os.environ.get("GDB_SERVER") # edit yo your case
SERVER_NAME = os.environ.get("MYSERVER")  # edit yo your case


def get_linker_script(app: str, memory: str) -> str:
    """Get the appropriate linker script path based on app and memory type."""
    base_path = PROJECT_ROOT / "utils" / "linker" / "STM32F4"

    if memory == "ram":
        return str(base_path / "stm32f4_ram.ld")
    return str(base_path / "stm32f4_flash.ld")


def get_cmake_source_dir(app: str) -> str:
    """Get the CMake source directory for the given app."""
    if app == "demo":
        return str(PROJECT_ROOT / "examples" / "STM32F401CCU")
    return str(PROJECT_ROOT / "tests")


def generate_gdb_script(build_dir: Path, memory: str) -> Path:
    """Generate a GDB script for load and debug operations."""
    gdb_script = build_dir / "firmware.gdb"

    ram_cmd = "monitor reset run"

    content = f"""\
# GDB script for load/debug ({memory} configuration)
#
# Variables (set via -ex before sourcing):
#   $DO_LOAD  - Load firmware (0 or 1)
#   $DO_DEBUG - Stay in GDB for debugging (0 or 1)

# Connect to target
target extended-remote {GDB_SERVER}:3333
monitor reset halt
tmx

# Load firmware if requested
if $DO_LOAD
    echo ==> Loading firmware...\\n
    load
    {ram_cmd}
end

# If not debugging, quit
if !$DO_DEBUG
    echo ==> Done.\\n
    monitor shutdown
    quit
end

echo ==> Debug session ready. Use 'continue' to run.\\n

define hook-quit
    tcln
    monitor shutdown
end
"""

    gdb_script.write_text(content)
    return gdb_script


def generate_app_script(build_dir: Path, app: str, memory: str) -> None:
    """Generate the app control script (demo.sh or tests.sh)."""
    elf_name = "test.elf"
    script_name = f"{app}.sh"
    script_path = build_dir / script_name
    run_block = """\
# Run mode - just monitor serial for RAM-loaded firmware
if [[ $DO_RUN -eq 1 ]]; then
    echo "==> Running tests..."
    ssh "$MYSERVER" bash -c '
        python3 ~/projects/libOBD2/scripts/serial_capture.py --timeout 30
    '
    exit $?
fi
"""

    # Generate GDB script
    generate_gdb_script(build_dir, memory)

    if memory == "flash":
        run_block = """\
if [[ $DO_RUN -eq 1 ]]; then
echo "==> Running tests..."
# Reset board and capture serial output with Python script
ssh "$MYSERVER" '
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "init; reset run; exit" 2>/dev/null
    sleep 3
    python3 ~/projects/libOBD2/scripts/serial_capture.py --timeout 30
'
exit $?
fi
"""

    script_content = f"""\
#!/bin/bash
# Auto-generated wrapper script for {app} ({memory} configuration)
# Usage: ./{script_name} [OPTIONS]
#
# Options:
#   -l, --load    Load/flash the firmware (uses GDB)
#   -r, --run     Run the firmware and monitor serial output
#   -d, --debug   Start debugging session (stays in GDB)
#
# Options can be combined: -lr (load and run), -ld (load and debug)
#
# Environment variables:
#   MYSERVER   - SSH host where the board is connected
#   GDB_SERVER - GDB server address

BUILD_DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"
ELF_FILE="${{BUILD_DIR}}/{elf_name}"
GDB_SCRIPT="${{BUILD_DIR}}/firmware.gdb"
GDB="gdb-multiarch"
MYSERVER="${{MYSERVER:-{SERVER_NAME}}}"
GDB_SERVER="${{GDB_SERVER:-{GDB_SERVER}}}"

DO_LOAD=0
DO_RUN=0
DO_DEBUG=0

# Parse arguments
for arg in "$@"; do
    case $arg in
        -l|--load)   DO_LOAD=1 ;;
        -r|--run)    DO_RUN=1 ;;
        -d|--debug)  DO_DEBUG=1 ;;
        -lr|-rl)     DO_LOAD=1; DO_RUN=1 ;;
        -ld|-dl)     DO_LOAD=1; DO_DEBUG=1 ;;
        -rd|-dr)     DO_RUN=1; DO_DEBUG=1 ;;
        -lrd|-ldr|-rld|-rdl|-dlr|-drl) DO_LOAD=1; DO_RUN=1; DO_DEBUG=1 ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -l, --load    Load/flash the firmware (uses GDB)"
            echo "  -r, --run     Run the firmware and monitor serial output"
            echo "  -d, --debug   Start debugging session (stays in GDB)"
            echo ""
            echo "Combined options:"
            echo "  -lr   Load and run"
            echo "  -ld   Load and debug"
            echo "  -lrd  Load, run, and debug"
            echo ""
            echo "Environment variables:"
            echo "  MYSERVER=$MYSERVER"
            echo "  GDB_SERVER=$GDB_SERVER"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            exit 1
            ;;
    esac
done

if [[ $DO_LOAD -eq 0 && $DO_RUN -eq 0 && $DO_DEBUG -eq 0 ]]; then
    echo "No action specified. Use -h for help."
    exit 1
fi

# Check if ELF file exists
if [[ ! -f "$ELF_FILE" ]]; then
    echo "Error: ELF file not found: $ELF_FILE"
    echo "Please build the project first: cd $BUILD_DIR && make"
    exit 1
fi

# Start OpenOCD on remote server if we need GDB (load or debug)
start_openocd() {{
    echo "==> Starting OpenOCD on $MYSERVER..."
    ssh "$MYSERVER" 'openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "bindto 0.0.0.0" -c "init; reset halt"' &
    OPENOCD_PID=$!
    sleep 1
}}

# Load and/or Debug mode - use GDB
if [[ $DO_LOAD -eq 1 || $DO_DEBUG -eq 1 ]]; then
    start_openocd

    $GDB "$ELF_FILE" \\
        -ex "set \\$GDB_SERVER=\\"$GDB_SERVER\\"" \\
        -ex "set \\$DO_LOAD=$DO_LOAD" \\
        -ex "set \\$DO_DEBUG=$DO_DEBUG" \\
        -x "$GDB_SCRIPT"

    # Cleanup OpenOCD
    kill $OPENOCD_PID 2>/dev/null

    # If also running, continue to run section
    if [[ $DO_RUN -eq 0 ]]; then
        exit 0
    fi
fi
{run_block}
"""

    script_path.write_text(script_content)
    # Make executable
    script_path.chmod(script_path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    print(f"  Generated: {script_path}")


def setup_build_directory(app: str, memory: str) -> None:
    """Set up the complete build directory for the given configuration."""
    # Create builds/<app>/ directory
    builds_dir = PROJECT_ROOT / "builds"
    build_dir = builds_dir /  f"{app}_{memory}"

    print(f"Setting up build directory: {build_dir}")
    print(f"  App: {app}")
    print(f"  Memory: {memory}")

    # Create directory
    build_dir.mkdir(parents=True, exist_ok=True)

    # Get paths
    linker_script = get_linker_script(app, memory)
    cmake_source = get_cmake_source_dir(app)

    # Generate custom toolchain with correct linker script
    toolchain_path = PROJECT_ROOT/"cmake/STM32F4.cmake"
    print(f"  Generated: {toolchain_path}")

    # Generate app control script
    generate_app_script(build_dir, app, memory)

    # Save configuration for reference
    config_path = build_dir / "build_config.txt"
    config_path.write_text(f"""\
# Build Configuration
APP={app}
MEMORY={memory}
LINKER_SCRIPT={linker_script}
CMAKE_SOURCE={cmake_source}
TOOLCHAIN={toolchain_path}
""")
    print(f"  Generated: {config_path}")

    # Run CMake
    print(f"\n==> Running CMake...")
    cmake_cmd = [
        "cmake",
        "-S", cmake_source,
        "-B", str(build_dir),
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path}",
        f"-DMEMORY={memory}"
    ]

    if "ON" == setup_build_directory.debug:
        cmake_cmd.append("-DDEBUG=ON")

    print(f"  Command: {' '.join(cmake_cmd)}")
    result = subprocess.run(cmake_cmd, cwd=build_dir)

    if result.returncode != 0:
        print("Error: CMake configuration failed!")
        sys.exit(1)

    print(f"\n==> Build directory ready: {build_dir}")
    print(f"    To build: cd {build_dir} && make")
    print(f"    To load:  cd {build_dir} && ./{app}.sh -l")
    print(f"    To run:   cd {build_dir} && ./{app}.sh -r")
    print(f"    To debug: cd {build_dir} && ./{app}.sh -d")


def main():
    parser = argparse.ArgumentParser(
        description="Generate CMake build directory for libOBD2 project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  %(prog)s --app=demo --memory=flash    # Build demo for flash
  %(prog)s --app=tests --memory=ram     # Build tests for RAM
  %(prog)s -a demo -M ram               # Short form
"""
    )

    parser.add_argument(
        "-a", "--app",
        choices=["demo", "tests"],
        required=True,
        help="Application to build (demo or tests)"
    )

    parser.add_argument(
        "-M", "--memory",
        choices=["flash", "ram"],
        default="flash",
        help="Memory target for firmware (default: flash)"
    )

    parser.add_argument(
        "--debug",
        choices=["ON", "OFF"],
        default="OFF",
        required=False,
        help="Enable debug mode (sets -DDEBUG=ON for CMake)"
    )

    args = parser.parse_args()

    # Pass debug flag to setup_build_directory via function attribute
    setup_build_directory.debug = args.debug
    setup_build_directory(args.app, args.memory)


if __name__ == "__main__":
    main()
