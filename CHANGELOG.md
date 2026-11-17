# Changelog

All notable changes to this project are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **CI checks before merge** (#69): Jenkins (`Jenkinsfile`) runs three checks on every PR and on `develop` / `main`, each reported as a GitHub status and each a script that runs locally: `ci/build-matrix` (`scripts/ci/build_matrix.py`: every port with every app it supports, Debug and Release, plus the single-header smoke test), `ci/unit-tests` (`scripts/ci/unit_tests.sh`: host_virtual unit tests must pass, and ignored tests may not increase) and `ci/coverage` (`scripts/ci/coverage.sh`: line / branch / function coverage may never drop below `tests/coverage_baseline.json`). See `scripts/ci/README.md`.
- `scripts/gen.py --platform / --memory / --type / --logging / --set KEY=VALUE / --build-root`: build any combination without editing `build_config.json`.
- `scripts/coverage.py --json`, `--baseline`, `--update-baseline`.
- **Single-header build** (#60): `scripts/amalgamate.py` and the `amalgamate` CMake target (`make amalgamate`) generate an stb-style `dist/libobd2.h`; define `LIBOBD2_IMPLEMENTATION` in one `.c` file. The script fails when two sources would clash in the single translation unit. Smoke test in `tests/amalgamation/` (`make amalgamate_check`, run by the CI build matrix); `-DLIBOBD2_USE_AMALGAMATION=ON` builds the `OBD2` library, and so the unit tests, from the generated header.
- Configurable protocols and timing: `LIBOBD2_SPT_ISO9141` / `LIBOBD2_SPT_KWP2000` can be defined by the user (ISO9141 stays the default), `LIBOBD2_STRICT_TIMING` selects the standard P1..P4 limits, and each `P*_TIME_*` limit can be overridden.
- **host_serial** port (`ports/host_serial/`): K-line on x86 Linux through a serial / USB adapter (KKL, FTDI), on top of libserialport. 5-baud and fast-init wake-ups use the serial BREAK condition; the port asks the driver for low latency so USB adapters do not batch received bytes.
- `examples/host_serial/`: `obd2_reader` command-line example that connects and reads VIN, CALID and coolant temperature.
- libserialport submodule (`examples/libs/libserialport`), built with CMake (`ports/host_serial/cmake/libserialport.cmake`).
- Fault injector for the `host_virtual` UART and timer ports (`ports/host_virtual/fault_injection.h`): declarative rules for UART errors / flipped bits on chosen bytes and failing, early-expiring or shifted timer operations; applied faults show on `kline.fault` in the VCD trace. First part of #46, still under review.

### Fixed

- The unit-test binary now exits with the number of failed tests instead of always 0.
- STM32 test builds no longer try to build `theft` (it needs POSIX `poll.h`); it is built for `host_virtual` only.
- `examples/stm32g4` builds again: it forces `SPT_FREERTOS=ON` like `examples/stm32f4`, and the stm32g4 toolchain uses `-std=gnu99` so TinyUSB's `asm` compiles.
- `examples/libs/cmsis_device_g4` and `ports/stm32g4/RTT` are listed in `.gitmodules`, so `git submodule update --init` works on a fresh clone.
- `scripts/gen.py` runs on Python 3.8 to 3.11 (no nested quotes in f-strings).
- Release (`NDEBUG`) builds no longer warn about parameters only used in `OBD2_DEBUG_ASSERT`; an unused-variable warning in `ISO9141_RecvMessage` is gone.
- `LibOBD2_Init` no longer reports a connection when no ECU answers: the stubbed KWP2000 StartCommunication (tried after ISO9141 fails) returned OK; it now returns `OBD_NOT_SUPPORTED` until KWP2000 is implemented.

### Changed

- `scripts/test-on-pr.sh` is removed; `scripts/ci/` replaces it.
- `datalink.h` no longer includes `utils.h` and `trace.h`; include them directly where their macros are used.
- **Breaking:** the `host` platform is renamed to `host_virtual` (`ports/host_virtual/`, `tests/src/**/host_virtual/`, define `PLATFORM_HOST_VIRTUAL`). Set `"PLATFORM": "host_virtual"` in `build_config.json` and delete `builds/library` once, since its CMake cache still points at the old toolchain path.

## [0.0.0](https://github.com/AndreiSaldorfean/libOBD2/releases/tag/v0.0.0) - 2026-06-24

First public preview. Expect breaking changes before `0.1.0`.

### Added

- Core **libOBD2** library (`LibOBD2_Init`, `LibOBD2_RequestService`)
- **ISO 9141-2** datalink (5-baud init, framing, checksums, connect/send/receive)
- **KWP2000** datalink skeleton and unit tests (not fully integrated)
- Layered architecture: app → datalink → UART/timing ports
- **FreeRTOS** optional integration (`SPT_FREERTOS`)
- Optional timing **trace** hooks (`SPT_TRACING`)
- Optional **LOGGING** compile flag (USB CDC paths on supported builds)
- **ECU simulator** (`ecu/`) for bench testing
- **STM32F4** port (UART, timer, startup, flash/RAM linker scripts)
- **STM32G4** port (early / less tested)
- **Example** firmware: `examples/stm32f4/` (FreeRTOS, TinyUSB)
- **On-target tests** with Unity (`tests/`) — datalink, ISO9141, KWP, app layer, ports (Not working properly, still needs work)
- **CMake** build for library, tests, and example
- `**build_config.json`** — central configuration for platform, memory, build type, feature flags
- `**utils/gen.py**` — generates CMake build directories from JSON config
- **Makefile** wrapper for configure (`make all`) and build (`make b`)

### Supported in this release


| Area                       | Status                                  |
| -------------------------- | --------------------------------------- |
| ISO 9141-2                 | Primary, tested on hardware             |
| STM32F401 + ISO 9141 Click | Reference setup                         |
| Build sytem                | `python3 utils/gen.py --app {library, example, test}`      |


### Known issues

- API is unstable; public headers are under `src/modules/`, not a dedicated install tree.
- KWP2000 service logic still has `#if 0` / TODO sections.
- `build_config.json` and Makefile `memory` / `build_type` must be kept in sync manually.
- Library configure may omit the ARM toolchain when both `SPT_FREERTOS` and `SPT_TRACING` are `OFF` (host `libOBD2.a` — not for firmware).
- `TYPE=Release` does not change `-O0` in the toolchain yet.
- No Makefile targets for flash, run, or debug (OpenOCD/GDB scripts removed in this build flow).
- README / docs site still minimal; no Doxygen or hosted docs yet.
- No CI workflow on the main repository.

### Dependencies (vendored)

Examples and tests bundle third-party code including **FreeRTOS**, **TinyUSB**, **libopencm3**, **CMSIS**, and **Unity**. See respective licenses under `examples/libs/` and `tests/Unity/`.
