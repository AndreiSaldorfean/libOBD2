# CI checks (#69)

Three checks run on every PR into `develop` / `main` and on every push to them (`Jenkinsfile`).
Branch protection requires all three, so a PR that breaks one cannot be merged. Each check is one
script that runs the same way on your machine:

| GitHub status | Command | Fails when |
| --- | --- | --- |
| `ci/build-matrix` | `python3 scripts/ci/build_matrix.py` | any port / app / build type in the matrix does not build |
| `ci/unit-tests` | `scripts/ci/unit_tests.sh` | a host_virtual unit test fails, the binary crashes, or more tests are `IGNORE`d than `tests/ignored_baseline.txt` |
| `ci/coverage` | `scripts/ci/coverage.sh` | line, branch or function coverage is below `tests/coverage_baseline.json` |

Outputs land in `builds/ci/` (logs, JUnit XML, coverage JSON) and `test_results/` (traces).

## Build matrix

`python3 scripts/ci/build_matrix.py --list` shows the cells; pass name filters to build a subset
(`build_matrix.py stm32f4 host_serial`). Each cell is configured with `scripts/gen.py` overrides
(`--platform/--memory/--type/--set/--build-root`), so `build_config.json` is never edited.

| Port | Cells |
| --- | --- |
| host (no port) | library Debug, Release |
| `host_virtual` | unit tests Debug, Release |
| `host_serial` | `obd2_reader` example Debug, Release |
| `stm32f4` | library with FreeRTOS + tracing; example flash Debug, Release; tests flash, ram |
| `stm32g4` | library with tracing; example flash; tests flash |
| single header (#60) | `make amalgamate_check`: `dist/libobd2.h` compiled and linked with gcc and arm-none-eabi-gcc |

STM32 cells need `arm-none-eabi-gcc` on `PATH` or `ARM_TOOLCHAIN_BIN=<toolchain>/bin`.
Cells in `KNOWN_BROKEN` (with their issue) are built but reported as skipped; today that is the
STM32 on-target test builds (#73).

## Coverage only goes up

`tests/coverage_baseline.json` holds the lowest allowed totals. A PR that adds untested code fails
`ci/coverage`. When a PR raises coverage, raise the baseline in the same PR, **to the totals CI
measured**: the `ci/coverage` status on the PR shows them ("lines ..%, branches ..%, functions ..%"),
and so do the `builds/ci/coverage.txt` / `coverage.json` artifacts of the Jenkins run. Copy them into
`tests/coverage_baseline.json` and push.

Do not take the values from a local run. gcov counts branches differently from one gcc version to
the next: on `ea80505` the same code measures 82.20% branch coverage with gcc 15 and 82.06% on the CI
server. A baseline from a newer local gcc can sit above what CI reaches, and `ci/coverage` then fails
on correct code (#84). `scripts/ci/coverage.sh --update` (never lowers a value) only gives CI's
numbers when run with the CI server's compiler.

Lowering the baseline by hand is a reviewed decision, never a way to make the check pass.
The same applies to `tests/ignored_baseline.txt`: an `IGNORE` is only for a `PBT_KNOWN_BUG`
property with a bug ticket; fixing one of those bugs lowers the number.

## Jenkins (192.168.0.100:8080)

- Multibranch job `libobd2` on `AndreiSaldorfean/libOBD2` (GitHub branch source): discovers
  `develop`, `main` and PRs (built merged with their target), rescans every 5 minutes because
  GitHub webhooks cannot reach the LAN server.
- Global properties: `ARM_TOOLCHAIN_BIN` (Arm GNU toolchain `bin/`), `GITHUB_STATUS_CREDENTIALS`
  (id of a username + token credential that can write commit statuses).
- Node tools: cmake, make, gcc/gcov, python3 >= 3.8, git.
