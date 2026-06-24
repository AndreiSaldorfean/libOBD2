# Changelog

All notable changes to this project are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
