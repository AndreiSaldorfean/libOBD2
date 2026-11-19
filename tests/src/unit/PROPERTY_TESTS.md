# Property-based tests

The example-based tests (`ex/`) check chosen scenarios. The property-based tests (`pbt/`, using
[theft](../../theft)) state what a layer **must guarantee for every input** and let theft search for an
input that breaks it. The aim is to find bugs, not to restate the implementation. Each property is
written from the protocol (ISO 9141-2, ISO 15031-5) or the layer's contract, never from what the code
currently does.

## How a property is built

1. **Invariant:** a sentence a reviewer can check against the spec, e.g. *"any valid ECU response
   inside the P1/P2 timing is returned exactly as sent"*.
2. **Generator:** builds inputs from theft's random stream: frames, timings, keyword bytes, message
   numbers, fault positions. Generators shrink: `PBT_Range()` shrinks towards the low end, so a failure
   is reported as the smallest input that still fails (e.g. all-zero data, the earliest allowed time).
   - Positive generators produce only **valid** inputs: inside every timing window, a correct
     checksum, at most 7 data bytes.
   - Negative generators take a valid input and break **exactly one thing** (one byte, one window,
     one field), so a failure points at one cause.
3. **Oracle:** the expected outcome comes from the spec. For a negative property it's usually "not
   OK", or the specific error code that names the fault.
4. **Run:** `PBT_Run()` (in `common/host_virtual/pbt_support.h`) uses a fixed seed, so every run and
   every CI run explores the same inputs. It's quiet unless a property fails, and it traces the
   virtual K-line into `test_results/`.

### Positive and negative still apply

A property is "for all inputs of a kind, the outcome is X", so the split carries over unchanged:

| folder | property shape | example |
|---|---|---|
| `positive/pbt` | for every **valid** input the layer succeeds with the right result | any 1..7-byte request goes out as header + data + checksum with P3/P4 timing |
| `negative/pbt` | for every **invalid / corrupted / mistimed** input the layer fails cleanly and never reports OK | any response with one byte changed is never accepted |

So the layout stays `<type>/<category>/` (see [README.md](README.md)); there was no reason to flip it.

### Known bugs: `PBT_KNOWN_BUG`

A property that states correct behaviour the library doesn't have yet runs with
`PBT_KNOWN_BUG(&cfg, trace, "Fn: ...")` instead of `PBT_ASSERT_HOLDS`:

- while theft still finds a counter-example, the test is reported **IGNORED** with the finding. The
  suite stays green, and every open bug shows in the summary (`N Ignored`);
- once no counter-example is found, the test **FAILS** with "fixed?", so whoever fixes the bug
  switches the test to `PBT_ASSERT_HOLDS` and it guards the fix from then on.

When a known bug would also break a broader positive property, the regression property skips exactly
the affected inputs (`THEFT_TRIAL_SKIP`, e.g. frames with an early checksum match). A separate
known-bug property covers them without the skip. Everything else stays guarded.

### Running

```sh
make b && make r                    # every property, default seed, ~10 s
PBT_SEED=0x1234 make r              # explore other inputs
PBT_TRIALS=5000 make r              # search harder
PBT_VERBOSE=1 make r                # also print the counter-examples of known bugs
```

A failing property prints its shrunk counter-example, and the seed to rerun it with.
`PBT_KNOWN_BUG` relies on the default seed: under another seed a rare bug may go unfound and show as
"fixed?".

## Invariants per layer

### Virtual drivers (test doubles the other layers stand on)

| property | invariant |
|---|---|
| `test_pos_pbt_TMR_DelayMs_0` | delays add up exactly; step and an armed timeout are untouched |
| `test_pos_pbt_TMR_IsTimeoutExpired_0` | expired exactly when the clock is past start + timeout; a stop always disarms |
| `test_pos_pbt_UART_RecvByte_0..2` | a byte is read back unchanged, never at its own write time; each poll costs one step |
| `test_pos_pbt_UART_RecvByte_3` | a burst of 1..32 bytes comes back complete, in order, none early, none extra |

### Datalink (`DL_ReadByteInTimeframe`, `DL_RecvByteBlocking`)

| property | invariant |
|---|---|
| `test_pos_pbt_DL_ReadByteInTimeframe_0..1` | a queued byte is returned; an empty line times out without a made-up byte |
| `test_pos_pbt_DL_ReadByteInTimeframe_2` | a byte inside `[timeMin, timeMax]` is OK, unchanged, and the elapsed time is the time taken |
| `test_pos_pbt_DL_RecvByteBlocking_0` | the blocking read returns the byte on the line |
| `test_neg_pbt_DL_ReadByteInTimeframe_0` | a missed byte gives `TIMEOUT_MAX` within one poll after `timeMax`; a late byte is kept for the next read |
| `test_neg_pbt_DL_ReadByteInTimeframe_1` | a timeout reports the time it took (**F2**) |
| `test_neg_pbt_DL_ReadByteInTimeframe_2` | a byte before `timeMin` is handed back flagged `TIMEOUT_MIN` |
| `test_neg_pbt_DL_ReadByteInTimeframe_3` | a byte with any UART error reports that error, never OK |
| `test_neg_pbt_DL_RecvByteBlocking_0` | a byte with a UART error is not handed out as good |

### ISO9141 (`ISO9141_Connect`, `_SendRequest`, `_RecvResponse`)

| property | invariant |
|---|---|
| `test_pos_pbt_ISO9141_ComputeChecksum_0`, `_SerializeMessage_0` | checksum is the byte sum; wire layout `fmt trg src sid params cs` |
| `test_pos_pbt_ISO9141_RecvMessage_0` | an unambiguous frame reads back unchanged |
| `test_pos_pbt_ISO9141_SendRequest_0` | any 1..7-byte request: `68 6A F1 data cs`, P3 before, P4 between, P2 armed at the end |
| `test_pos_pbt_ISO9141_RecvResponse_0` | any valid response (1..7 data, P1/P2 timing) is returned exactly (**F1**) |
| `test_pos_pbt_ISO9141_Connect_0` | an ECU inside every W1..W4 window with kb `08 08` / `94 94` connects with the matching header |
| `test_neg_pbt_ISO9141_RecvResponse_0` | a response with one byte changed is never accepted (**F1**) |
| `test_neg_pbt_ISO9141_RecvResponse_1` | a response whose header is not `48 6B` is not accepted (**F3**) |
| `test_neg_pbt_ISO9141_RecvResponse_2` | a response with more than 7 data bytes is rejected (**F4**) |
| `test_neg_pbt_ISO9141_RecvResponse_3` | an ECU pause over P1 max ends the frame with `DATA_BYTE_NOT_RECVD` |
| `test_neg_pbt_ISO9141_RecvResponse_4` | a response before P2 min is rejected with the P2 min timeout |
| `test_neg_pbt_ISO9141_RecvResponse_5` | sid `7F` is always reported as `ECU_RESPONSE_7F` |
| `test_neg_pbt_ISO9141_Connect_0` | one wrong or mistimed init byte (10 kinds) fails with the error naming it, no protocol or header left behind |
| `test_neg_pbt_ISO9141_SendRequest_0` | a request length of 0 or 8..65535 is refused before anything goes on the line |

### App (`LibOBD2_RequestService`)

| property | invariant |
|---|---|
| `test_pos_pbt_LIBOBD2_RequestService_0` | a VIN of any length is reassembled across $09 messages (mocked data link) |
| `test_pos_pbt_LIBOBD2_RequestService_1` | VIN/CALID of 1..9 messages come back complete and in order over the K-line; nothing written past the result |
| `test_pos_pbt_LIBOBD2_RequestService_2` | any coolant byte decodes to the `int16_t` `A - 40` |
| `test_pos_pbt_LIBOBD2_RequestService_3` | any data the ECU sends reaches the caller (**F1** at app level) |
| `test_neg_pbt_LIBOBD2_RequestService_0` | a transport send failure is returned unchanged; nothing is received |
| `test_neg_pbt_LIBOBD2_RequestService_1` | $09 data is assembled by message number, or the request fails (**F5**) |
| `test_neg_pbt_LIBOBD2_RequestService_2` | an answer with the wrong sid or PID is never OK (**F6**) |
| `test_neg_pbt_LIBOBD2_RequestService_3` | a coolant answer without its data byte is rejected (**F7**, fixed) |
| `test_neg_pbt_LIBOBD2_RequestService_4` | an answer for another PID or service is never decoded as coolant |
| `test_neg_pbt_LIBOBD2_RequestService_5` | a UART error on any ECU byte of an exchange fails it with no data |

## Findings

Each finding is a `PBT_KNOWN_BUG` property and has a bug ticket (#62 to #68); the counter-examples are the shrunk ones theft reports.

### F1 (#62): a data byte equal to the running checksum ends the frame early
- **Where:** `src/datalink/specific/iso9141/iso9141_recv.c:99`. `ISO9141_RecvMessage` frames a response
  by reading until a byte equals the sum of the bytes before it.
- **Counter-examples:**
  - `48 6B 04 0B C2 84`: after `0B` the running sum is `C2`, so the data byte `C2` is taken as the
    checksum. The frame comes back OK with one data byte instead of two, and `84` stays on the line.
  - Corrupting `48 6B 00 00 00 B3` to `48 6B 00 00 B3 B3` is accepted as a valid 1-byte frame
    (`test_neg_pbt_ISO9141_RecvResponse_0`), so a corrupted response is returned as OK.
- **Impact:** about one valid frame in 256 per data byte is cut, and the leftover bytes spoil the next
  read. At app level some VINs and calibration IDs can't be read at all (e.g. a VIN message
  `49 02 07 17 …`), and coolant `A = 0x0B` reads as −40 °C.
- **Fix direction:** end the frame on timing, not content. ISO 9141-2 ends a message when no byte
  follows within P1 max (or once P2 min passes). Then verify the last byte as the checksum. A
  checksum match should confirm a frame, not end it.

### F2 (#63): a timed-out read reports a wrong elapsed time
- **Where:** `src/datalink/common/datalink_utils.h:122`. On the timeout path `lTimeElapsed = timeEnd
  - timeStart` runs while `timeEnd` is still 0.
- **Counter-example:** start 0, window `[0, 0]`, no byte: the call takes 2 ms and reports 0. With the
  clock at T it reports `0 − T` (wrapped).
- **Impact:** none today (no caller reads it after a timeout), but anything budgeting a protocol window
  from it would go wrong.
- **Fix direction:** read the clock on the timeout path too.

### F3 (#64): the response header is never checked
- **Where:** `ISO9141_RecvMessage` / `ISO9141_RecvResponse_Verify` only look at the checksum and sid `7F`.
- **Counter-example:** `49 6B 00 00 B4` (format byte `49` instead of `48`) is returned as OK.
- **Impact:** frames that aren't ECU responses to the tester are accepted. The loopback echo of the
  tester's own request (`68 6A F1 …`) is one; an adapter that doesn't drop the echo makes the tester
  "answer itself".
- **Fix direction:** accept only `48 6B <src>` (store the ECU address from the first response).

### F4 (#65): frames with more than 7 data bytes are accepted
- **Where:** `src/datalink/specific/iso9141/iso9141_priv.h:17`. `ISO9141_MAX_DATA_FIELD_LEN` is
  `sizeof(obd_data_t) + 1` (13), not 7 + checksum.
- **Counter-example:** `48 6B 00` + 8 × `00` + `B3` is returned as OK with 8 data bytes.
- **Impact:** over-long frames are passed up; the app decoders then read params the spec doesn't define.
- **Fix direction:** limit the data field to 7 bytes (+ checksum) and report `RECV_MSG_FAILED` beyond.

### F5 (#66): $09 message numbers are ignored
- **Where:** `src/app/inc/libobd2_formulas.h:232`. `LibOBD2_FormulaMultiFrame` joins messages in arrival
  order and skips the message-number byte without checking it.
- **Counter-example:** VIN messages numbered `2 1 3 4 5` are joined in that order and returned OK.
- **Impact:** a repeated, missing or reordered message gives a wrong VIN/CALID with an OK status.
- **Fix direction:** place each message by its number and fail on a duplicate, a gap or a number
  outside `1..count`.

### F6 (#67): $09 answers are not matched to the request
- **Where:** `src/app/src/libobd2_requests.c:156`. `TP_MSG_CNT` takes the count from `param[1]` of any answer, and
  `TP_MSGS` stores whatever comes next. Only the first message's sid/PID is looked at, when decoding.
- **Counter-example:** a count answer `48 01 05` (sid `48`, PID `01`) is taken as "5 VIN messages" and
  the request returns OK. A CALID message among VIN messages is joined in too.
- **Impact:** data from another request, or a garbled answer, is returned as the VIN.
- **Fix direction:** check sid = request + `0x40` and PID = requested PID on every answer, including
  the count answer.

### F7 (#68): a coolant answer without its data byte decodes anyway — fixed
- **Fixed by:** the PID formula table (`LIBOBD2_PID_FORMULAS` in `src/app/inc/libobd2_formulas.h`) gives
  the number of data bytes each PID needs; `LibOBD2_DecodePid` fails a shorter answer with
  `OBD_ERR_APP_SHORT_MSG`. The property now runs with `PBT_ASSERT_HOLDS`.
- **Where:** `src/app/libobd2.c:148`, `LibOBD2_FormulaCoolantTemp`: `param[1]` is read without checking
  `dataLen`.
- **Counter-example:** the answer `41 05` returns OK, 1 byte, `0 − 40` = −40 °C.
- **Impact:** a truncated answer reports a plausible but invented temperature.
- **Fix direction:** require `dataLen >= 3` (sid, PID, A) before decoding; in general check the
  length each PID needs.

## Notes on the test bench (not library bugs)

- **The virtual line echoes everything the tester sends**, including the bit-banged 5-baud address,
  and `UART_FlushRx` is a no-op. ECU bytes must be queued before the tester runs, so the 5-baud
  properties place the inverted address from a model of the tester's timing. Each trial then checks
  that the tester really sent ~KB2 at the modeled time (`DLFX_PBT_SentAt`), so a wrong model can't
  hide a bug.
- **Poll resolution:** a byte is visible 1 ms after it lands, and the tester checks its timeout right
  after a poll that misses. A byte landing exactly at a window's max is therefore missed. The generators
  keep valid times one or two polls inside each window. That's a property of the 1 ms virtual poll,
  not a protocol requirement.
- **Queue size:** the virtual line holds 128 bytes (`VIRT_UART_QUEUE_SIZE`) and silently overwrites
  the oldest one, so an exchange is limited to 9 $09 messages.
- **The virtual timer isn't wrap-safe** (`timeoutTime < timeNow`); generated start times stay below
  2³¹. The real ports compare start + duration and are wrap-safe.
- **Timing config:** every property uses the P/W macros, so it holds under both timing blocks in
  `src/datalink/common/datalink.h` (P1/P2 max 5000 ms, or the real 20/50 ms). Both are checked.
- Fixed along the way: the three property tests that were broken on `develop` (#57). One mock link
  had no driver pointers, one mock put VIN data at the wrong param index, and one link had no
  protocol context. Also removed the unused, buggy `alloc_time` generators.
