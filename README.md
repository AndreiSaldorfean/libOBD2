<h1 align="center">
  <img width="1536" height="284" alt="libOBD2_logo" src="https://github.com/user-attachments/assets/6f5d57d5-e018-4def-8afb-e8b2d87b05d9" alt="libOBD2"/>

  <a href="CHANGELOG.md">Changelog</a>
</h1>


![GitHub forks](https://img.shields.io/github/forks/AndreiSaldorfean/libOBD2?style=flat)
![GitHub Repo stars](https://img.shields.io/github/stars/AndreiSaldorfean/libOBD2?style=flat)
![GitHub Issues or Pull Requests](https://img.shields.io/github/issues/AndreiSaldorfean/libOBD2?style=flat)
![GitHub License](https://img.shields.io/github/license/AndreiSaldorfean/libOBD2?style=flat)
![GitHub last commit](https://img.shields.io/github/last-commit/AndreiSaldorfean/libOBD2)

> **v0.0.0 — experimental.** API and build flow may change. ISO 9141 on STM32F4 is the main tested path.

## 📌 Description

**libOBD2** is a lightweight library written in **C**, that aims to create an easy to understand and extend library, for the **OBD2** protocol.

It is split into layers:

- **App** — `LibOBD2_Init`, `LibOBD2_RequestService`
- **Datalink** — ISO 9141-2 (primary), KWP2000 (WIP)
- **Ports** — UART and timer hooks for your MCU

---

## 🚀 Features

- ISO 9141-2 datalink (5-baud init, messaging, checksums)
- KWP2000 datalink (partial)
- Pluggable UART / timing interfaces
- Optional FreeRTOS (`SPT_FREERTOS`) and tracing (`SPT_TRACING`)
- On-target Unity unit tests
- ECU simulator for bench testing
- Example firmware for STM32F401 (`examples/stm32f4/`)
- CMake build + JSON config (`build_config.json`)

---

## 📦 Building

**Requirements:** CMake ≥ 3.14, Python 3, Make, `arm-none-eabi-gcc`, OpenOCD (for flash/debug)

```bash
# First time setup
git clone https://github.com/AndreiSaldorfean/libOBD2.git
cd libOBD2
git submodule update --init --recursive

# libopencm3 (needed for tests & example)
cd examples/libs/libopencm3 && make TARGETS=stm32/f4 && cd ../../..
```

**Configure** — edit `build_config.json` (platform, memory, build type, flags) and `Makefile`:

```makefile
build_type=Release     # must match "TYPE" in build_config.json
memory=flash           # must match "MEMORY" in build_config.json
app=tests              # library | tests | example
```

**Build:**

```bash
make all    # configure (runs utils/gen.py)
make b      # compile
```

| App | Output |
|-----|--------|
| `library` | `builds/library/libOBD2.a` |
| `tests` | `builds/tests_flash_Release/test.elf` |
| `example` | `builds/example_flash_Release/demo.elf` |

Or directly: `python3 utils/gen.py --app tests`

---

## ⚡ How to use (minimal example)

Wire your port layer into a datalink handle, then call the API:

```c
#include "libobd2.h"

obd_ctx_t ctx = {
    .pDataLink = &myDataLink,   /* UART + timer ops, ISO9141 callbacks */
    .connectionStatus = false,
};

if (LibOBD2_Init(&ctx) != OBD_STATUS_OK) { /* error */ }

uint8_t response[64];
size_t len = 0;
LibOBD2_RequestService(&ctx, OBD_GET_COOLANT_TEMP, response, &len);
```

Full wiring: see `examples/stm32f4/` and `ports/stm32f4/`.

---

## 📊 Stats

| | |
|---|---|
| **Version** | 0.0.0 (preview) |
| **License** | MIT |
| **Primary MCU** | STM32F401 |
| **Primary protocol** | ISO 9141-2 |
| **Tests** | Unity, on-target |

---

## ℹ️ My setup

Currently this is my hardware with which i test the library:

- [Stm32f401ccu (Blackpill)](https://wiki.kamamilabs.com/index.php?title=KAmod_BlackPill_411)
- [ISO 9141 CLICK](https://www.mikroe.com/iso-9141-click) - for the uart to k-line transceiver
- **Raspberry PI zero 2 W** - for displaying the data.
- **ESP32 C3 Super Mini** - as the tester, the repo for esp support will be soon available.

Flash `test.elf` or `demo.elf` with OpenOCD / your IDE after `make b`.

---

#### ☕ Support My Work

If you enjoy my projects and want to support me, you can do so through ko-fi:

[![ko-fi](https://img.shields.io/badge/-Ko%20Fi-FFDD00?style=for-the-badge&logo=ko-fi&logoColor=black)](https://ko-fi.com/andreisaldorfean)

#### 📬 Contact

For information, job offers, collaboration, sponsorship, you can contact me via email.

📧 Email: andreisaldorfean@gmail.com

#### 👨‍💻 Tech and Standards i use

- [SemVer](https://semver.org/) - Versioning Scheme
- [Neovim](https://github.com/neovim/neovim) - Text Editor

---
