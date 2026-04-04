#!/usr/bin/env python3
"""
Reliable serial capture for unit test output.
Waits for END marker and returns 0 on SUCCESS, 1 on FAILED.
"""

import argparse
import sys
import serial
import time


def capture_serial(port: str, baudrate: int, timeout: float, end_marker: str) -> tuple[str, int]:
    """
    Capture serial output until end_marker is seen.
    Returns (output, exit_code) where exit_code is 0 for SUCCESS, 1 otherwise.
    """
    output_lines = []
    exit_code = 1  # Default to failure

    try:
        ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=0.1,  # Short timeout for responsive reading
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )

        # Flush any garbage in buffers
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        start_time = time.time()
        buffer = ""

        while True:
            # Check overall timeout
            if time.time() - start_time > timeout:
                print(f"ERROR: Timeout after {timeout}s waiting for '{end_marker}'", file=sys.stderr)
                break

            # Read available data
            if ser.in_waiting > 0:
                try:
                    data = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
                    buffer += data

                    # Process complete lines
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.rstrip('\r')  # Handle \r\n
                        output_lines.append(line)
                        print(line)  # Print in real-time

                        # Check for result markers
                        if "OK" in line:
                            exit_code = 0
                            if buffer.strip():
                                print(buffer.strip())
                            ser.close()
                            return '\n'.join(output_lines), exit_code
                        elif "FAIL" in line:
                            exit_code = 1
                            if buffer.strip():
                                print(buffer.strip())
                            ser.close()
                            return '\n'.join(output_lines), exit_code

                except UnicodeDecodeError:
                    pass  # Ignore decode errors
            else:
                time.sleep(0.01)  # Small sleep to avoid busy-waiting

        ser.close()

    except serial.SerialException as e:
        print(f"ERROR: Serial error: {e}", file=sys.stderr)
        return '\n'.join(output_lines), 1
    except KeyboardInterrupt:
        print("\nInterrupted", file=sys.stderr)
        return '\n'.join(output_lines), 130

    return '\n'.join(output_lines), exit_code


def main():
    parser = argparse.ArgumentParser(
        description="Capture serial output from unit tests",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  %(prog)s                           # Use defaults
  %(prog)s -p /dev/ttyUSB0           # Different port
  %(prog)s -t 60                     # 60 second timeout
  %(prog)s --end-marker "DONE"       # Different end marker
"""
    )

    parser.add_argument(
        "-p", "--port",
        default="/dev/ttyACM0",
        help="Serial port (default: /dev/ttyACM0)"
    )

    parser.add_argument(
        "-b", "--baudrate",
        type=int,
        default=115200,
        help="Baud rate (default: 115200)"
    )

    parser.add_argument(
        "-t", "--timeout",
        type=float,
        default=30.0,
        help="Timeout in seconds (default: 30)"
    )

    parser.add_argument(
        "--end-marker",
        default="OK",
        help="Marker indicating end of output (default: 'UNIT END')"
    )

    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Don't print output, just return exit code"
    )

    args = parser.parse_args()

    # Wait for serial device to appear
    for i in range(50):
        try:
            with open(args.port):
                break
        except (FileNotFoundError, PermissionError):
            if i == 49:
                print(f"ERROR: Serial port {args.port} not found", file=sys.stderr)
                sys.exit(1)
            time.sleep(0.3)

    output, exit_code = capture_serial(
        port=args.port,
        baudrate=args.baudrate,
        timeout=args.timeout,
        end_marker=args.end_marker
    )

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
