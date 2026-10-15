#ifndef VCD_WRITER_H
#define VCD_WRITER_H

/*************************************************************************************************************
 * VCD writer.
 *
 * Usage:
 *   1. VCD_WriterConfigDefault(), adjust, then VCD_WriterOpenFile() or VCD_WriterOpenSink().
 *   2. Declare signals with VCD_ScopeBegin() / VCD_AddVar() / VCD_ScopeEnd(). Declarations are only
 *      allowed before the first value change; scopes left open are closed automatically.
 *   3. Record value changes with VCD_ChangeBit/Bool/U64/Bits/Real/String() or VCD_Pulse().
 *   4. VCD_WriterClose() writes everything still buffered and frees the writer.
 *
 * Ordering modes (vcdWriterConfig_t.order):
 *   VCD_ORDER_STRICT  Changes must come in non-decreasing time. A change at time t writes out
 *                     everything before t. Memory use stays constant. A change earlier than the
 *                     previous one fails with VCD_ERR_TIME (the writer stays usable).
 *   VCD_ORDER_SORTED  Changes may come in any time order and are sorted before writing. Call
 *                     VCD_Commit(t) to promise that nothing earlier than t will follow: everything
 *                     before t is then written out and freed. Without commits the whole trace stays
 *                     in memory until VCD_WriterClose().
 *
 * Rules for changes at the same time:
 *   - Several changes of one signal at the same time collapse to the one recorded last.
 *   - With dropRedundant (default) a change to the value the signal already has is not written, so
 *     drawing every bit of a protocol frame only costs the real edges.
 *
 * The first written timestamp carries a $dumpvars block with the start value of every signal; digital
 * signals that have no change at that time start as 'x'.
 *
 * Errors: VCD_ERR_IO and VCD_ERR_NOMEM are sticky - every later call returns them and VCD_WriterClose()
 * reports them. All other errors only reject that one call.
 *
 * Not thread safe: use one writer per thread or serialize the calls.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "vcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================= MACROS ================================================ */
#define VCD_SIGNAL_NONE (-1)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct vcdWriter vcdWriter_t;

/* Handle of a declared signal. */
typedef int32_t vcdSignal_t;

/* Output callback: consume 'len' bytes, return false on failure (the writer then reports VCD_ERR_IO).
 * Lets the writer stream anywhere: a socket, a UART, semihosting, a memory buffer. */
typedef bool (*vcdSinkFn_t)(void *ctx, const char *data, size_t len);

typedef enum
{
    VCD_ORDER_STRICT = 0,
    VCD_ORDER_SORTED,
} vcdOrder_t;

typedef struct
{
    vcdTimescale_t timescale;     /* tick length, magnitude 1/10/100. Default 1 ns */
    vcdOrder_t     order;         /* default VCD_ORDER_STRICT */
    bool           dropRedundant; /* skip changes that keep the current value. Default true */
    const char    *date;          /* NULL: current local time, "": no $date section */
    const char    *version;       /* NULL: "VCDio <version>", "": no $version section */
    const char    *comment;       /* NULL or "": no $comment section */
} vcdWriterConfig_t;

/* =============================================== MODULE API ============================================== */
void VCD_WriterConfigDefault(vcdWriterConfig_t *cfg);

/* Creates/truncates 'path'. cfg may be NULL for defaults. */
vcdStatus_t VCD_WriterOpenFile(vcdWriter_t **writer, const char *path, const vcdWriterConfig_t *cfg);

/* Streams the output through 'sink'. cfg may be NULL for defaults. */
vcdStatus_t VCD_WriterOpenSink(vcdWriter_t **writer, vcdSinkFn_t sink, void *ctx, const vcdWriterConfig_t *cfg);

/* Writes everything still buffered, then an end marker '#endTime' (pass VCD_TIME_NONE for none) so the
 * last values have a visible duration, and frees the writer. Always frees, even when returning an error.
 * VCD_ERR_TIME if endTime is earlier than the last written time (the file is still complete). */
vcdStatus_t VCD_WriterClose(vcdWriter_t *writer, vcdTime_t endTime);

/* Opens a module scope; signals declared until the matching VCD_ScopeEnd() show up inside it.
 * Names may not contain whitespace. */
vcdStatus_t VCD_ScopeBegin(vcdWriter_t *writer, const char *name);
vcdStatus_t VCD_ScopeEnd(vcdWriter_t *writer);

/* Declares a signal in the current scope.
 *   VCD_VAR_WIRE, VCD_VAR_REG : width 1..65535 bits
 *   VCD_VAR_INTEGER           : width 1..65535 bits, 0 means 32
 *   VCD_VAR_REAL, VCD_VAR_STRING : width is ignored
 * The name may carry a bit range ("data[7:0]") but no whitespace. 'signal' may be NULL. */
vcdStatus_t VCD_AddVar(
    vcdWriter_t *writer,
    vcdVarType_t type,
    uint32_t width,
    const char *name,
    vcdSignal_t *signal);

/* 1-bit digital signals: '0', '1', 'x' or 'z' (upper case accepted). */
vcdStatus_t VCD_ChangeBit(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, char value);
vcdStatus_t VCD_ChangeBool(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, bool value);

/* Digital signals of any width. VCD_ERR_VALUE if the value has bits above the signal width. */
vcdStatus_t VCD_ChangeU64(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, uint64_t value);

/* Digital signals of any width, MSB first, characters 0 1 x z. A shorter string is extended on the left
 * as VCD does it: with 0 when its first character is 0 or 1, otherwise with that x or z. */
vcdStatus_t VCD_ChangeBits(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, const char *bits);

/* VCD_VAR_REAL signals. NaN and infinity are rejected with VCD_ERR_VALUE. */
vcdStatus_t VCD_ChangeReal(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, double value);

/* VCD_VAR_STRING signals. Whitespace and control characters are replaced by '_'. */
vcdStatus_t VCD_ChangeString(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, const char *text);

/* 1-bit signals: '1' at 'time', '0' at time + width. In strict mode this moves the write position to
 * time + width, so record other signals' changes inside the pulse first. */
vcdStatus_t VCD_Pulse(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, vcdTime_t width);

/* Promises that no change earlier than 'time' will follow, writing out and freeing everything before it.
 * Later changes before 'time' fail with VCD_ERR_TIME. */
vcdStatus_t VCD_Commit(vcdWriter_t *writer, vcdTime_t time);

/* Pushes already written output to the sink/file (does not commit pending changes). */
vcdStatus_t VCD_Flush(vcdWriter_t *writer);

/* Sticky error state (VCD_OK while healthy). */
vcdStatus_t VCD_WriterStatus(const vcdWriter_t *writer);

vcdTimescale_t VCD_WriterTimescale(const vcdWriter_t *writer);

#ifdef __cplusplus
}
#endif

#endif /* VCD_WRITER_H */
