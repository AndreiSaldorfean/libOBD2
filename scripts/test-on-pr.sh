#!/bin/bash
set -e  # Exit on any error

# Get the project root directory (parent of scripts/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Testing unit tests..."
echo "Project root: $PROJECT_ROOT"

# Sync submodule URLs (in case they were cached with SSH URLs)
git submodule sync
git submodule update --init --recursive

make && make b

cd $PROJECT_ROOT/examples/libs/libopencm3/ && make TARGETS='stm32/f4'

cd $PROJECT_ROOT && make test_r && make test_rb

# Load to RAM (not flash) and run
cd "$PROJECT_ROOT/builds/tests_ram" && openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "init; reset halt; load_image libOBD2.elf; reset run; exit"

# Wait for USB CDC to enumerate
echo "Waiting for serial port..."
for i in {1..20}; do
    [ -e /dev/ttyACM0 ] && break
    sleep 0.5
done

# Capture serial output with reliable Python script
python3 $PROJECT_ROOT/scripts/serial_capture.py --timeout 30

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo "✓ All tests passed!"
else
    echo "✗ Tests failed!"
fi

exit $EXIT_CODE
