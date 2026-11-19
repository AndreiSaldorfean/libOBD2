# Unit tests (host_virtual)

Tests run on the host against the virtual K-line (`ports/host_virtual`): a loopback UART and a virtual
millisecond clock, so whole ISO9141 exchanges run in microseconds and can be traced.

```sh
make all     # configure (app=tests in the Makefile)
make b       # build
make r       # run: ./builds/tests_flash_Debug/test (known bugs show as IGNORED, see PROPERTY_TESTS.md)
make cc      # coverage build + run + report
```

## Layout

Every layer keeps its tests in `<type>/<category>/`, next to the fixture they share:

```text
common/host_virtual/
    pbt_support.c/.h      shared property-test runner, generators, PBT_KNOWN_BUG
drivers/host_virtual/
    positive/ex/    timer_test, uart_test, fault_injection_test
    positive/pbt/   timer_pbt_test, uart_pbt_test
    negative/ex/    timer_neg_test, uart_neg_test, fault_injection_neg_test
datalink/host_virtual/
    dl_fixture.c/.h       virtual line + ECU byte queueing for all folders
    dl_pbt_support.c/.h   ECU response / 5-baud init generators for the property tests
    positive/ex/    dl_test, iso9141_test
    positive/pbt/   dl_pbt_test, iso9141_pbt_test
    negative/ex/    dl_neg_test, dl_fault_test, iso9141_neg_test
    negative/pbt/   dl_pbt_neg_test, iso9141_pbt_neg_test
app/host_virtual/
    libobd2_fixture.c/.h  connected tester + ECU frame / SID $09 queueing, mock links, scripted ECU
    positive/ex/    libobd2_test
    positive/pbt/   libobd2_pbt_test
    negative/ex/    libobd2_neg_test
    negative/pbt/   libobd2_pbt_neg_test
```

Type (what is expected of the code):

- **positive**: the code is used as intended and must succeed (valid requests, correct ECU answers,
  boundary values that are still allowed).
- **negative**: the code must fail cleanly: invalid arguments, a missing or wrong ECU answer, timing
  violations, UART errors and timer faults (see `ports/host_virtual/fault_injection.h`).

Category (how the test checks it):

- **ex** (example-based): one chosen input and its expected output, e.g. "the ECU answers
  `55 08 08 CC`, so Connect returns OK".
- **pbt** (property-based, [theft](../../theft)): a rule that must hold for every input, checked on
  many generated inputs, e.g. "any byte written can be read back unchanged". How they are built, the
  invariants of each layer and the bugs they found: [PROPERTY_TESTS.md](PROPERTY_TESTS.md).

The two are independent: a property test is still positive or negative. Only create a folder when it
holds tests.

`ports/host_virtual/cmake/unitTests.cmake` compiles every `.c` in these folders, so a new test file only
needs its functions added to `targetRunTests()` in `tests/src/ports/host_virtual/target_test_fixtures.c`
(positive tests first, then negative, each group driver -> datalink -> app). File names carry the type
and category too (`<module>_test`, `<module>_neg_test`, `<module>_pbt_test`, `<module>_pbt_neg_test`):
all test folders share one include path, so every header name must be unique.

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
void test_neg_ex_DL_Connect_0(void)
```

The folder gives the type, so the header does not repeat it.

## Test names

`test_<type>_<category>_<MODULE>_<Function>_<n>`, e.g. `test_pos_ex_DL_Connect_0`,
`test_neg_ex_DL_Connect_0`, `test_pos_pbt_UART_RecvByte_0`.

- `<type>` is `pos` / `neg` and `<category>` is `ex` / `pbt`, matching the folders the test lives in.
- `<MODULE>` is the module prefix: `TMR`, `UART`, `FAULT`, `DL`, `DLFAULT`, `ISO9141`, `LIBOBD2`.
- `<n>` counts the tests of one type, category and function from 0, in run order. A new test takes
  the next free number; what it checks goes in its `brief:`.

Name the trace after the test (`UART_TRACE_Begin("test_neg_ex_DL_Connect_0.vcd")`), so `test_results/`
sorts by type, then category, then module.

## Waveform traces

`UART_TRACE_Begin("test_X.vcd")` ... `UART_TRACE_End()` (see `tests/src/ports/host_virtual/uart_trace.h`)
records the virtual K-line. Relative names land in `test_results/` in the directory the test binary runs
from (the repository root with `make r`). Each trace comes with a `test_X.pvs`, and PulseView loads it
automatically when you open or import `test_X.vcd`, so the UART decoders are set already:

- **K-line**: RX = `line` (what was on the wire), TX = `rx_data` (what the library read), 10400 baud 8N1
- **5-baud init**: RX = `line` at 5 baud, only in traces that contain the bit-banged wake-up address

```sh
pulseview -i test_results/test_pos_ex_DL_Connect_0.vcd -I vcd
```
