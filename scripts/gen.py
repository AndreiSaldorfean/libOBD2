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
build_root = PROJECT_ROOT / "builds"


def config_to_cmake_args(config: dict) -> list[str]:
    args = []

    platform = build_config["PLATFORM"]
    if app != "library" or (app == "library" and (build_config[app]["SPT_TRACING"] == "ON" or build_config[app]["SPT_FREERTOS"] == "ON")):
        toolchain = f"{PROJECT_ROOT}/ports/{platform}/cmake/toolchain.cmake"
        args.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
        args.append(f"-DCMAKE_BUILD_TYPE={build_config['TYPE']}")

    args.append(f"-DMEMORY={build_config['MEMORY']}")
    args.append(f"-DLOGGING={build_config['LOGGING']}")
    if app == "library" and build_config[app]["SPT_TRACING"] == "ON":
        args.append(f"-DPLATFORM={build_config['PLATFORM']}")
    elif app != "library":
        args.append(f"-DPLATFORM={build_config['PLATFORM']}")

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


def create_cfg(app: str, coverage: bool = False) -> None:
    build_dir = build_root / f"{app}_{build_config['MEMORY']}_{build_config['TYPE']}"

    if coverage:
        # Coverage gets its own build directory so the normal test build stays uninstrumented
        if app != "tests" or build_config["PLATFORM"] != "host_virtual":
            print("Error: --coverage needs --app tests and PLATFORM host_virtual", file=sys.stderr)
            sys.exit(1)
        config = dict(build_config[app], COVERAGE="ON")
        run_cmake(common_cmake_cfg(PROJECT_ROOT / "tests", build_dir.with_name(build_dir.name + "_cc"), config))
        return

    match app:
        case "library":
            run_cmake(common_cmake_cfg(PROJECT_ROOT, build_root / "library", build_config[app]))
        case "example":
            run_cmake(common_cmake_cfg(PROJECT_ROOT, build_root / "library", build_config[app]))
            run_cmake(common_cmake_cfg(PROJECT_ROOT / "examples" / build_config["PLATFORM"], build_dir, build_config[app]))
        case "tests":
            run_cmake(common_cmake_cfg(PROJECT_ROOT, build_root / "library", build_config[app]))
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
    parser.add_argument("--coverage", action="store_true",
                        help="configure the tests in builds/<dir>_cc with gcov instrumentation (host_virtual only)")
    # Overrides of build_config.json, so CI can build any combination without editing tracked files
    parser.add_argument("--platform", help="override PLATFORM (host_virtual, host_serial, stm32f4, stm32g4)")
    parser.add_argument("--memory", choices=["flash", "ram"], help="override MEMORY")
    parser.add_argument("--type", choices=["Debug", "Release"], help="override TYPE (CMake build type)")
    parser.add_argument("--logging", choices=["ON", "OFF"], help="override LOGGING")
    parser.add_argument("--set", action="append", default=[], metavar="KEY=VALUE",
                        help="override an option of the app section, e.g. --set SPT_TRACING=ON (repeatable)")
    parser.add_argument("--build-root", type=Path,
                        help="directory that holds the build directories (default: builds/)")
    return parser.parse_args()


def apply_overrides(config: dict, args) -> dict:
    for key, value in (("PLATFORM", args.platform), ("MEMORY", args.memory),
                       ("TYPE", args.type), ("LOGGING", args.logging)):
        if value is not None:
            config[key] = value
    if args.platform and not (PROJECT_ROOT / "ports" / args.platform).is_dir():
        print(f"Error: no port ports/{args.platform}", file=sys.stderr)
        sys.exit(1)
    config[args.app] = dict(config[args.app])
    for item in args.set:
        key, sep, value = item.partition("=")
        if not sep or not key:
            print(f"Error: --set expects KEY=VALUE, got '{item}'", file=sys.stderr)
            sys.exit(1)
        config[args.app][key] = value
    return config


if __name__ == "__main__":
    args = parse_arguments()
    app = args.app
    build_config = apply_overrides(parse_build_config(app), args)
    if args.build_root:
        build_root = args.build_root.resolve()
    create_cfg(app, args.coverage)

