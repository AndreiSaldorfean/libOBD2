#!/usr/bin/env bash
# Coverage ratchet (#69): line, branch and function coverage of the library (host_virtual tests)
# may never drop below tests/coverage_baseline.json. Same command locally and in Jenkins:
#   scripts/ci/coverage.sh            # check
#   scripts/ci/coverage.sh --update   # raise the baseline to this run's totals (never lowers them)
# Raise the committed baseline to the totals CI measured (the ci/coverage status of the PR), not to a
# local run: other gcc versions count branches differently (scripts/ci/README.md, #84).
# Output: builds/ci/coverage.txt (report), builds/ci/coverage.json (totals + per file)
set -euo pipefail
cd "$(dirname "$0")/../.."

OUT=builds/ci/coverage
DIR="$OUT/tests_flash_Debug_cc"
BASELINE=tests/coverage_baseline.json
rm -rf "$OUT"
mkdir -p builds/ci

python3 scripts/gen.py --app tests --platform host_virtual --memory flash --type Debug --build-root "$OUT" --coverage
cmake --build "$DIR" -j"$(nproc)"
python3 scripts/coverage.py --reset "$DIR"
# Test failures are reported by the unit-test stage; here only the counters matter.
"$DIR/test" > builds/ci/coverage_tests.txt 2>&1 || echo "note: some tests failed during the coverage run"

if [[ "${1:-}" == "--update" ]]; then
    python3 scripts/coverage.py "$DIR" --json builds/ci/coverage.json --update-baseline "$BASELINE" | tee builds/ci/coverage.txt
else
    python3 scripts/coverage.py "$DIR" --json builds/ci/coverage.json --baseline "$BASELINE" | tee builds/ci/coverage.txt
    exit "${PIPESTATUS[0]}"
fi
