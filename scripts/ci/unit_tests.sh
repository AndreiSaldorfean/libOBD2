#!/usr/bin/env bash
# host_virtual unit tests (#69): build, run, JUnit report. Same command locally and in Jenkins:
#   scripts/ci/unit_tests.sh
# Fails when the build fails, when any test fails, or when the number of ignored tests goes
# up compared with tests/ignored_baseline.txt (IGNORE is only for PBT_KNOWN_BUG properties).
# Output: builds/ci/unit_tests.txt (Unity log), builds/ci/unit_tests.xml (JUnit), test_results/ (traces)
set -euo pipefail
cd "$(dirname "$0")/../.."

OUT=builds/ci/unit
BIN="$OUT/tests_flash_Debug/test"
rm -rf "$OUT" test_results
mkdir -p builds/ci

python3 scripts/gen.py --app tests --platform host_virtual --memory flash --type Debug --build-root "$OUT"
cmake --build "$OUT/tests_flash_Debug" -j"$(nproc)"

set +e
"$BIN" | tee builds/ci/unit_tests.txt
rc=${PIPESTATUS[0]}
set -e

python3 scripts/ci/unity_junit.py builds/ci/unit_tests.txt builds/ci/unit_tests.xml \
    --max-ignored "$(cat tests/ignored_baseline.txt 2>/dev/null || echo 0)" || rc=$((rc ? rc : 1))
exit "$rc"
