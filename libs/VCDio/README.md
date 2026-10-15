# VCDio

Read and write **Value Change Dump** (`.vcd`) waveform files in C99, with built-in UART framing and decoding.

VCD is the plain-text waveform format from IEEE 1364 (Verilog) / IEEE 1800 clause 21.7. GTKWave, Surfer,
PulseView/sigrok and practically every simulator open it. VCDio lets C code produce such files, for example
to turn what a host test pushed through a virtual UART into a waveform with a protocol decoder on top, and
read them back, for example to assert in a test that a trace decodes to the expected bytes.

- **Writer**: scopes, wire/reg/integer/real/string signals, streaming or time-sorted output, redundant change
  removal, output to a file or any callback (socket, UART, semihosting, memory).
- **Reader**: loads VCD from VCDio, GTKWave, Icarus, Verilator, GHDL, sigrok and others; value-at-time lookups by binary search.
- **UART**: draws frames (5–9 data bits, parity, 1/1.5/2 stop bits, MSB/LSB first, inverted lines, error
  injection) and decodes frames from a trace like a logic analyzer.
- **`vcdio` tool**: `info`, `dump` and `uart` decoding from the command line.
- No dependencies beyond the C standard library (and `libm`). Allocator replaceable. Warning-clean with
  `-Wall -Wextra -Wpedantic -Wconversion`, tested under ASan/UBSan.

## Build

```sh
cmake -S libs/VCDio -B build/vcdio
cmake --build build/vcdio
ctest --test-dir build/vcdio          # unit tests
./build/vcdio/vcdio_kline_example     # writes kline_example.vcd
```

As a dependency of another CMake project (tests, tools and examples are then off by default):

```cmake
add_subdirectory(libs/VCDio)
target_link_libraries(my_target PRIVATE VCDio::VCDio)
```

| Option | Default | |
|---|---|---|
| `VCDIO_BUILD_TESTS` | ON when top level | unit tests (`vcdio_tests`) |
| `VCDIO_BUILD_TOOLS` | ON when top level | `vcdio` command line tool |
| `VCDIO_BUILD_EXAMPLES` | ON when top level | `vcdio_kline_example` |
| `VCDIO_NO_TIME` | OFF | don't call `time()`/`localtime()` for `$date` (targets without a clock) |

Custom allocator: compile the library with `-DVCD_MALLOC=... -DVCD_REALLOC=... -DVCD_FREE=...` (all three).

## Writing a trace

```c
#include "vcd_io.h"

vcdWriterConfig_t cfg;
VCD_WriterConfigDefault(&cfg);
cfg.timescale = (vcdTimescale_t){1, VCD_UNIT_US};   /* 1 tick = 1 us */

vcdWriter_t *w;
VCD_WriterOpenFile(&w, "trace.vcd", &cfg);

vcdSignal_t tx, state;
VCD_ScopeBegin(w, "kline");
VCD_AddVar(w, VCD_VAR_WIRE, 1, "tx", &tx);
VCD_AddVar(w, VCD_VAR_STRING, 0, "state", &state);
VCD_ScopeEnd(w);

VCD_ChangeBit(w, tx, 0, '1');
VCD_ChangeString(w, state, 0, "IDLE");
VCD_ChangeBit(w, tx, 100, '0');
VCD_ChangeString(w, state, 100, "SENDING");

VCD_WriterClose(w, 1000);   /* 1000 = end marker, so the last values have a duration */
```

Every function returns a `vcdStatus_t`; `VCD_StatusStr()` turns it into text.

### Ordering modes

| `cfg.order` | Changes must arrive | Memory | Use when |
|---|---|---|---|
| `VCD_ORDER_STRICT` (default) | in non-decreasing time | constant | one producer walking forward in time |
| `VCD_ORDER_SORTED` | in any order | grows until `VCD_Commit(t)` | several producers (tx and rx side), events logged out of order |

In sorted mode `VCD_Commit(t)` promises nothing earlier than `t` will come and writes out everything before
it. Call it from time to time on long runs to keep memory bounded.

### Same-time and redundant changes

- Several changes of one signal at the same time collapse to the last one recorded.
- With `cfg.dropRedundant` (default) a change to the value a signal already has is not written. That is
  why the UART helper can write every bit and the file still only holds real edges.
- The first timestamp carries a `$dumpvars` block; digital signals without a value there start as `x`.

## UART

```c
vcdUartConfig_t uart;
VCD_UartConfigDefault(&uart, cfg.timescale, 10400);   /* 8N1, bit = 96.15 ticks */
uart.dataSignal = txData;                              /* optional 8-bit signal mirroring the byte */

VCD_UartWriteIdle(w, tx, 0, &uart);
VCD_UartWriteFrame(w, tx, 1000, 0x33, &uart, 0, &end);
VCD_UartWriteBytes(w, tx, end + 5000, request, sizeof request, 5000 /* gap */, &uart, &end);
VCD_UartWriteFrame(w, tx, end + 1000, 0x5A, &uart, VCD_UART_FLAG_FRAMING_ERROR, NULL);
VCD_UartWriteBreak(w, tx, start, duration, &uart);
```

Fractional bit periods are fine: every edge is placed at `round(frameStart + n * bitTicks)`, so long
frames don't drift. In strict mode frames on different lines must not overlap in time; use sorted mode for
full-duplex traffic.

Open the result in **PulseView** (Import → Value Change Dump, then add a UART decoder on `tx`/`rx`) to see
decoded bytes, or in **GTKWave/Surfer**, where `dataSignal` and string signals show the values directly.
PulseView only imports 1-bit signals; it skips string and vector signals with a warning.

## Reading a trace

```c
vcdReader_t *r;
vcdParseError_t err;
if (VCD_ReaderOpenFile(&r, "trace.vcd", &err) != VCD_OK)
    printf("line %zu: %s\n", err.line, err.message);

size_t tx;
VCD_ReaderFind(r, "kline.tx", &tx);          /* full name, or leaf name if unique */
const char *v = VCD_ReaderValueAt(r, tx, 1234);   /* "0", "1", "x", ... */

for (size_t i = 0; i < VCD_ReaderChangeCount(r, tx); i++) {
    vcdTime_t t; const char *value;
    VCD_ReaderChange(r, tx, i, &t, &value);
}
VCD_ReaderClose(r);
```

Values come back as text without the type prefix: `"1"`, `"1010"` (use `VCD_ValueToU64()`), `"2.5"`,
`"IDLE"`. The whole file is loaded into memory.

### Decoding UART from a trace

```c
static bool onFrame(void *ctx, const vcdUartFrame_t *f)
{
    printf("%02X %s\n", f->data, f->errors ? "error" : "ok");
    return true;   /* false stops decoding */
}

vcdUartConfig_t uart;
VCD_UartConfigDefault(&uart, VCD_ReaderTimescale(r), 10400);
VCD_UartDecode(r, tx, &uart, onFrame, NULL);
```

Reported errors: `VCD_UART_ERR_FRAMING` (stop bit low), `VCD_UART_ERR_PARITY`, `VCD_UART_ERR_UNKNOWN`
(`x`/`z` sampled). A break shows up as `0x00` with a framing error, as on real hardware.

## Command line

```text
$ vcdio info kline_example.vcd
timescale  1 us
time       0.000 s .. 2.650 s

signal                                   type       width id        changes
kline.tx                                 wire           1 !              39
kline.rx                                 wire           1 "              81
...

$ vcdio uart kline_example.vcd rx 10400
# kline_example.vcd  kline.rx  10400 baud  8N1
         start             gap    data   chr  status
       2.360 s               -    0x55  U    ok
       2.371 s       10.000 ms    0x08  .    ok
...
       2.599 s       20.000 ms    0x5A  Z    framing
# 15 frames, 1 with errors

$ vcdio dump trace.vcd kline.state
```

`vcdio uart` options: `-d 5..9` data bits, `-p n|e|o|m|s` parity, `-s 1|1.5|2` stop bits, `-m` MSB
first, `-i` inverted. Exit code 3 means frames with errors were found, so scripts can use it as a check.

## VCD in 30 seconds

```text
$timescale 1us $end               <- unit of every #time
$scope module kline $end
$var wire 1 ! tx $end             <- type, width, short id, name
$upscope $end
$enddefinitions $end
#0                                <- time = 0 us
$dumpvars
1!                                <- tx = 1 (1-bit: value + id, no space)
$end
#100
0!                                <- at 100 us tx = 0
#196
1!
```

Vectors are written `b1010 id` (with a space), reals `r2.5 id`, strings `sTEXT id`. Only changes are
stored; a signal keeps its value until the next change.

## Limits

- Not thread safe; use one writer/reader per thread.
- Reals are printed with `%.17g`, so keep the C locale (`LC_NUMERIC`) at its default `"C"`.
- String values cannot contain whitespace (the writer replaces it with `_`).
- The reader keeps the whole trace in memory (about 16 bytes per change plus vector text).
