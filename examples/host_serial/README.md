# host_serial example: `obd2_reader`

Reads OBD2 data from a car on an x86 Linux host, through a serial or USB K-line adapter
(a "KKL" / FTDI cable). It uses the `ports/host_serial` port, which is built on
[libserialport](https://sigrok.org/wiki/Libserialport) (`examples/libs/libserialport`).

## Build

```sh
git submodule update --init examples/libs/libserialport
```

In `build_config.json`, set `"PLATFORM": "host_serial"`, then:

```sh
python scripts/gen.py --app example
cmake --build builds/example_flash_Debug
```

## Run

```sh
./builds/example_flash_Debug/obd2_reader --list          # serial ports found
./builds/example_flash_Debug/obd2_reader /dev/ttyUSB0    # connect and read VIN, CALID, coolant temp
```

Your user needs access to the port (on most distros, membership of the `dialout` group).

## How the port drives the K-line

| libOBD2 call                      | host_serial                                                         |
|-----------------------------------|---------------------------------------------------------------------|
| `send_pulse(low / high)`          | serial BREAK on / off: TX held low, then released to idle high      |
| `switch_mode(*_BEGIN / *_START)`  | waits for TX to finish, line idle high                              |
| `switch_mode(*_END)`              | releases BREAK and drops the echo the wake-up left on RX            |
| `flush_rx`                        | waits for TX, gives the K-line echo `flushSettleMs` (default 5 ms) to come back, drops RX |
| `recv_byte`                       | waits up to 1 ms for a byte; per-byte parity/framing errors are not reported |

`UART_Init` sets `ASYNC_LOW_LATENCY` on the port. Without it, FTDI adapters hold received
bytes for up to 16 ms, and the ISO9141 inter-byte timing checks (e.g. W2 ≥ 5 ms) then fail.
If your adapter's driver ignores the flag, set the latency timer yourself:

```sh
echo 1 | sudo tee /sys/bus/usb-serial/devices/ttyUSB0/latency_timer
```
