#!/bin/bash
# Compiles and links the single-header smoke test (impl.c + user.c) with host GCC
# and arm-none-eabi-gcc, in a few configurations, and runs the host binary.
# Usage: tests/amalgamation/check.sh <dir holding libobd2.h> [out dir]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST="$(cd "${1:?usage: check.sh <dir holding libobd2.h> [out dir]}" && pwd)"
OUT="${2:-$DIST/smoke}"
mkdir -p "$OUT"

[ -f "$DIST/libobd2.h" ] || { echo "check.sh: $DIST/libobd2.h not found" >&2; exit 1; }

CFLAGS=(-Wall -Wextra -Werror -std=c99 -I"$DIST")
ARM_FLAGS=(-mcpu=cortex-m4 -mthumb --specs=nosys.specs)
# Configurations a user can pick with -D flags, see the banner of libobd2.h
CONFIGS=(
    ""
    "-DNDEBUG -O2"
    "-DLIBOBD2_STRICT_TIMING"
    "-DLIBOBD2_SPT_ISO9141 -DLIBOBD2_SPT_KWP2000"
)

for cfg in "${CONFIGS[@]}"; do
    echo "== gcc ${cfg:-(default)}"
    # shellcheck disable=SC2086
    gcc "${CFLAGS[@]}" $cfg "$HERE/impl.c" "$HERE/user.c" -o "$OUT/smoke_host"
    "$OUT/smoke_host"

    echo "== arm-none-eabi-gcc ${cfg:-(default)}"
    # The newlib nosys stubs warn at link time; only compile errors matter here
    # shellcheck disable=SC2086
    arm-none-eabi-gcc "${CFLAGS[@]}" "${ARM_FLAGS[@]}" $cfg "$HERE/impl.c" "$HERE/user.c" \
        -o "$OUT/smoke_arm.elf" 2> "$OUT/arm_link.log" \
        || { cat "$OUT/arm_link.log" >&2; exit 1; }
done

echo "Single header OK"
