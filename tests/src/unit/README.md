# Unit tests (host_virtual)

Tests run on the host against the virtual K-line (`ports/host_virtual`): a loopback UART and a virtual
millisecond clock, so whole ISO9141 exchanges run in microseconds and can be traced.

```sh
make all     # configure (app=tests in the Makefile)
make b       # build
make r       # run: ./builds/tests_flash_Debug/test
make cc      # coverage build + run + report
```

## Layout

Every layer keeps its tests in two folders, next to the fixture both of them share:

```text
drivers/host_virtual/
    positive/   timer_test, uart_test, fault_injection_test, uart_pbt_test
    negative/   timer_neg_test, uart_neg_test, fault_injection_neg_test
datalink/host_virtual/
    dl_fixture.c/.h       virtual line + ECU byte queueing for both folders
    positive/   dl_test, iso9141_test, dl_pbt_test, iso9141_pbt_test
    negative/   dl_neg_test, dl_fault_test, iso9141_neg_test
app/host_virtual/
    libobd2_fixture.c/.h  connected tester + ECU frame / SID $09 queueing
    positive/   libobd2_test, libobd2_pbt_test
    negative/   libobd2_neg_test, libobd2_pbt_neg_test
```

- **positive**: the code is used as intended and must succeed (valid requests, correct ECU answers,
  boundary values that are still allowed).
- **negative**: the code must fail cleanly: invalid arguments, a missing or wrong ECU answer, timing
  violations, UART errors and timer faults (see `ports/host_virtual/fault_injection.h`).

`ports/host_virtual/cmake/unitTests.cmake` compiles every `.c` in these folders, so a new test file only
needs its functions added to `targetRunTests()` in `tests/src/ports/host_virtual/target_test_fixtures.c`
(positive tests first, then negative, each group driver -> datalink -> app). Name negative files
`<module>_neg_test.c`.

## Test header

Every test function starts with this header. Number the steps and mark them in the body with
`/* 1. */`, `/* 2. */` ... so the header and the code can be read side by side.

```c
/******************************************************************
 * - brief:       One line: what the test proves
 * - description: The scenario and why it matters, wrapped at the
 *                same indent. ECU bytes go here too, e.g.
 *                ECU: 48 6B 12 41 05 7B 86
 * - steps:
 *     1. Set-up / stimulus
 *     2. Call under test and its expected result
 *     3. Side effects that must (not) have happened
 ******************************************************************/
void test_neg_DL_Connect_0(void)
```

The folder gives the type, so the header does not repeat it.

## Test names

`test_<pos|neg>_<MODULE>_<Function>_<n>`, e.g. `test_pos_DL_Connect_0`, `test_neg_DL_Connect_0`,
`test_neg_ISO9141_Connect_0`.

- `pos` / `neg` matches the folder the test lives in.
- `<MODULE>` is the module prefix: `TMR`, `UART`, `FAULT`, `DL`, `DLFAULT`, `ISO9141`, `LIBOBD2`
  (property tests add `_PBT`, e.g. `test_pos_UART_PBT_RecvByte_0`).
- `<n>` counts the tests of one function and type from 0, in run order. A new test takes the next
  free number; what it checks goes in its `brief:`.

Name the trace after the test (`UART_TRACE_Begin("test_neg_DL_Connect_0.vcd")`), so `test_results/` sorts
into `test_neg_*` and `test_pos_*` and each module's traces sit together.

## Waveform traces

`UART_TRACE_Begin("test_X.vcd")` ... `UART_TRACE_End()` (see `tests/src/ports/host_virtual/uart_trace.h`)
records the virtual K-line. Relative names land in `test_results/` in the directory the test binary runs
from (the repository root with `make r`). Each trace comes with a `test_X.pvs`, and PulseView loads it
automatically when you open or import `test_X.vcd`, so the UART decoders are set already:

- **K-line**: RX = `line` (what was on the wire), TX = `rx_data` (what the library read), 10400 baud 8N1
- **5-baud init**: RX = `line` at 5 baud, only in traces that contain the bit-banged wake-up address

```sh
pulseview -i test_results/test_pos_DL_Connect_0.vcd -I vcd
```
