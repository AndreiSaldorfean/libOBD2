"""
GDB Python script: dump and pretty-print the in-memory trace buffer.

Usage from GDB prompt:
    (gdb) source /home/rudy/Projects/libOBD2/scripts/dump_trace.py
    (gdb) dump_trace          -- print last N entries (default 512)
    (gdb) dump_trace 64       -- print last 64 entries

The output columns are:
    SEQ      head-relative sequence number (0 = oldest shown)
    UART     which UART peripheral (U1/U2/-)
    TAG      trace tag (TX/RX/FLUSH/TMR_*/…)
    DATA     the data byte in hex and decimal
    CYC      full 32-bit DWT->CYCCNT snapshot at log time
    DELTA    microseconds since the previous entry (any kind)
    IO_GAP   microseconds since the previous TX or RX operation
             (blank for the first TX/RX; shown on every entry so you can
             see how long each step takes relative to the last bus event)
    NOTE     extra decoded information

At 84 MHz: 1 µs = 84 cycles.  DELTA and IO_GAP are both shown in µs.
"""

import gdb

TAGS = {
    0x01: "TX      ",
    0x02: "RX      ",
    0x03: "FLUSH   ",
    0x04: "TIMEOUT ",
    0x05: "TMR_STA ",   # StartTimeout(max_ms)  - data = timeMax
    0x06: "TMR_STP ",   # StopTimeout
    0x07: "TMR_MAX!",   # expired: MAX breach   - data = timeMax that was set
    0x08: "TMR_MIN!",   # elapsed < timeMin     - data = actual elapsed ms
    0x09: "TMR_OK  ",   # byte in window        - data = actual elapsed ms
    0x0F: "MARK    ",
}

# IO operations: TX and RX
IO_TAGS = {0x01, 0x02}

# Tags that indicate a timing violation (highlighted with a marker)
VIOLATION_TAGS = {0x07, 0x08}

CPU_MHZ = 84  # STM32F401 @ 84 MHz → 1 µs = 84 cycles

def cyc_to_us(delta_cyc):
    """Convert a (wrapped) 32-bit cycle delta to microseconds."""
    return (delta_cyc & 0xFFFFFFFF) // CPU_MHZ

class DumpTrace(gdb.Command):
    """dump_trace [N]  -- print last N entries from the ring trace buffer"""

    def __init__(self):
        super().__init__("dump_trace", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        n = int(arg.strip()) if arg.strip() else 512

        # Read head index
        head    = int(gdb.parse_and_eval("g_trace_head"))
        buf_len = int(gdb.parse_and_eval("sizeof(g_trace_buf) / sizeof(g_trace_buf[0])"))

        n     = min(n, buf_len)
        start = head - n  # may be negative; masking handles wrap
        mask  = buf_len - 1

        hdr = (f"{'SEQ':>5}  {'UART':>4}  {'TAG':<8}  {'DATA':>10}  "
               f"{'CYC':>12}  {'DELTA(µs)':>10}  {'IO_GAP(µs)':>11}  NOTE")
        print(hdr)
        print("-" * len(hdr))

        prev_cyc    = None   # cycle count of previous entry (any tag)
        last_io_cyc = None   # cycle count of previous TX or RX entry

        for i in range(n):
            idx   = (start + i) & mask
            entry = gdb.parse_and_eval(f"g_trace_buf[{idx}]")
            tag_raw = int(entry["tag"])
            data    = int(entry["data"])
            cyc     = int(entry["cyc"])

            if tag_raw == 0:
                continue  # unwritten slot

            uart_id  = (tag_raw >> 4) & 0x0F
            op_tag   = tag_raw & 0x0F
            uart_str = f"U{uart_id}" if uart_id else "  -"
            tag_str  = TAGS.get(op_tag, f"0x{op_tag:01x}      ")

            # --- DELTA: µs since previous entry (any type) ---
            if prev_cyc is not None:
                delta_us = cyc_to_us(cyc - prev_cyc)
                delta_str = f"{delta_us:>10,}"
            else:
                delta_str = f"{'—':>10}"
            prev_cyc = cyc

            # --- IO_GAP: µs since previous TX or RX ---
            if op_tag in IO_TAGS:
                if last_io_cyc is not None:
                    io_gap_us  = cyc_to_us(cyc - last_io_cyc)
                    io_gap_str = f"{io_gap_us:>11,}"
                else:
                    io_gap_str = f"{'—':>11}"
                last_io_cyc = cyc
            else:
                # For non-IO entries show the gap from the last IO event
                if last_io_cyc is not None:
                    io_gap_us  = cyc_to_us(cyc - last_io_cyc)
                    io_gap_str = f"{io_gap_us:>11,}"
                else:
                    io_gap_str = f"{'—':>11}"

            # --- NOTE: human-readable annotation ---
            if op_tag == 0x05:
                note = f"max={data} ms"
            elif op_tag in (0x07, 0x08, 0x09):
                note = f"elapsed={data} ms"
            elif op_tag in IO_TAGS:
                note = f"0x{data:02x} ({data:3d})"
            else:
                note = ""

            violation = "  <== VIOLATION" if op_tag in VIOLATION_TAGS else ""

            print(f"{i:>5}  {uart_str:>4}  {tag_str}  0x{data:02x} ({data:3d})  "
                  f"{cyc:>12}  {delta_str}  {io_gap_str}  {note}{violation}")

DumpTrace()
print("dump_trace loaded. Usage: dump_trace [N]")
