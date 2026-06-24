import argparse
import json
import os
import sys
import subprocess
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_CONFIG_PATH = PROJECT_ROOT / "build_config.json"

app = "library"
build_config = {}


def config_to_cmake_args(config: dict) -> list[str]:
    args = []

    platform = build_config["PLATFORM"]
    if app != "library" or (app == "library" and (build_config[app]["SPT_TRACING"] == "ON" or build_config[app]["SPT_FREERTOS"] == "ON")):
        toolchain = f"{PROJECT_ROOT}/ports/{platform}/cmake/toolchain.cmake"
        args.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
        args.append(f"-DCMAKE_BUILD_TYPE={build_config["TYPE"]}")

    args.append(f"-DMEMORY={build_config['MEMORY']}")
    args.append(f"-DLOGGING={build_config['LOGGING']}")
    if app == "library" and build_config[app]["SPT_TRACING"] == "ON":
        args.append(f"-DPLATFORM={build_config["PLATFORM"]}")
    elif app != "library":
        args.append(f"-DPLATFORM={build_config["PLATFORM"]}")

    for key, value in config.items():
        args.append(f"-D{key}={value}")

    return args


def common_cmake_cfg(cmake_source: Path, build_dir: Path, config: dict) -> list[str]:
    os.makedirs(build_dir, exist_ok=True)
    return [
        "cmake",
        "-S", str(cmake_source),
        "-B", str(build_dir),
        "-G", "Unix Makefiles",
        *config_to_cmake_args(config),
    ]


def run_cmake(cmake_cmd: list[str]) -> None:
    print(f"  Command: {' '.join(cmake_cmd)}")
    result = subprocess.run(cmake_cmd, cwd=PROJECT_ROOT)

    if result.returncode != 0:
        print("Error: CMake configuration failed!", file=sys.stderr)
        sys.exit(1)


def create_cfg(app: str) -> None:
    build_dir = PROJECT_ROOT / "builds" / f"{app}_{build_config["MEMORY"]}_{build_config["TYPE"]}"
    match app:
        case "library":
            run_cmake(common_cmake_cfg(PROJECT_ROOT, PROJECT_ROOT / "builds" / "library", build_config[app]))
        case "example":
            run_cmake(common_cmake_cfg(PROJECT_ROOT, PROJECT_ROOT / "builds" / "library", build_config[app]))
            run_cmake(common_cmake_cfg(PROJECT_ROOT / "examples" / build_config["PLATFORM"], build_dir, build_config[app]))
        case "tests":
            run_cmake(common_cmake_cfg(PROJECT_ROOT, PROJECT_ROOT / "builds" / "library", build_config[app]))
            run_cmake(common_cmake_cfg(PROJECT_ROOT / "tests", build_dir, build_config[app]))
        case _:
            print(f"Error: unsupported app '{app}'", file=sys.stderr)
            sys.exit(1)


def parse_build_config(app: str) -> dict:
    if not BUILD_CONFIG_PATH.is_file():
        print(f"Error: build config not found: {BUILD_CONFIG_PATH}", file=sys.stderr)
        sys.exit(1)

    with BUILD_CONFIG_PATH.open(encoding="utf-8") as f:
        all_configs = json.load(f)

    if app not in all_configs:
        available = ", ".join(sorted(all_configs))
        print(f"Error: unknown app '{app}'. Available: {available}", file=sys.stderr)
        sys.exit(1)

    return dict(all_configs)


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Generate CMake build directory for libOBD2 project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("-a", "--app", choices=["example", "tests", "library"], help="", required=True)
    return parser.parse_args()


if __name__ == "__main__":
    app = parse_arguments().app
    build_config = parse_build_config(app)
    create_cfg(app)
