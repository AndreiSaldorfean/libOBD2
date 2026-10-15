#ifndef VCD_UART_H
#define VCD_UART_H

/*************************************************************************************************************
 * UART helpers.
 *
 * Writing: expands data words into line levels on a 1-bit signal - start bit, 5..9 data bits, optional
 * parity, 1/1.5/2 stop bits - so viewers with a UART decoder (PulseView, sigrok-cli) show the bytes.
 * Errors can be injected per frame (wrong parity, missing stop bit) and a vector signal can mirror the
 * frame value for viewers without decoders (GTKWave, Surfer).
 *
 * Reading: decodes the frames back out of a 1-bit variable of a vcdReader_t, like a logic analyzer:
 * finds idle->start edges and samples each bit in its middle.
 *
 * Bit timing is given in timescale ticks and may be fractional (10400 baud at 1 us is 96.15 ticks);
 * edges are placed at round(start + n * bitTicks), so frames never drift.
 *
 * In VCD_ORDER_STRICT mode, frames on different lines must not overlap in time, because a frame is
 * written from its start to its end. Use VCD_ORDER_SORTED for full duplex traffic.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "vcd_reader.h"
#include "vcd_types.h"
#include "vcd_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================= MACROS ================================================ */
/* VCD_UartWriteFrame() flags: inject errors into the frame */
#define VCD_UART_FLAG_PARITY_ERROR  (1u << 0) /* send the wrong parity bit (needs parity enabled) */
#define VCD_UART_FLAG_FRAMING_ERROR (1u << 1) /* hold the stop bit at space level, idle again after it */

/* vcdUartFrame_t.errors bits */
#define VCD_UART_ERR_PARITY  (1u << 0) /* parity bit does not match */
#define VCD_UART_ERR_FRAMING (1u << 1) /* stop bit is not at mark level */
#define VCD_UART_ERR_UNKNOWN (1u << 2) /* a data/parity/stop sample was x, z or similar */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef enum
{
    VCD_UART_PARITY_NONE = 0,
    VCD_UART_PARITY_ODD,
    VCD_UART_PARITY_EVEN,
    VCD_UART_PARITY_MARK,  /* always 1 */
    VCD_UART_PARITY_SPACE, /* always 0 */
} vcdUartParity_t;

typedef enum
{
    VCD_UART_STOP_1 = 0,
    VCD_UART_STOP_1_5,
    VCD_UART_STOP_2,
} vcdUartStop_t;

typedef struct
{
    double          bitTicks;   /* bit period in timescale ticks, see VCD_UartBitTicks(); >= 1 */
    uint8_t         dataBits;   /* 5..9 */
    vcdUartParity_t parity;
    vcdUartStop_t   stopBits;
    bool            msbFirst;   /* false: LSB first, as standard UARTs send */
    bool            inverted;   /* false: idle/mark is '1' and the start bit '0' (TTL UART, K-line) */
    vcdSignal_t     dataSignal; /* writer only: digital signal (>= dataBits wide) that shows the frame
                                   value while the frame is on the line and 'z' otherwise, or
                                   VCD_SIGNAL_NONE */
} vcdUartConfig_t;

typedef struct
{
    vcdTime_t start;  /* falling edge of the start bit */
    vcdTime_t end;    /* end of the last stop bit */
    uint16_t  data;
    uint32_t  errors; /* VCD_UART_ERR_* bits, 0 for a clean frame */
} vcdUartFrame_t;

/* Called for every decoded frame. Return false to stop decoding. */
typedef bool (*vcdUartFrameFn_t)(void *ctx, const vcdUartFrame_t *frame);

/* =============================================== MODULE API ============================================== */

/* Bit period in ticks of 'ts' for a baud rate, e.g. ({1, VCD_UNIT_US}, 10400) -> 96.15. 0 when invalid. */
double VCD_UartBitTicks(vcdTimescale_t ts, double baud);

/* 8N1, LSB first, not inverted, no data signal. */
void VCD_UartConfigDefault(vcdUartConfig_t *cfg, vcdTimescale_t ts, double baud);

/* Length of one frame (start to end of stop bits) in ticks. */
vcdTime_t VCD_UartFrameTicks(const vcdUartConfig_t *cfg);

/* Drives the line to its idle (mark) level at 'time'. Use it at time 0 so the line does not start as 'x'. */
vcdStatus_t VCD_UartWriteIdle(vcdWriter_t *writer, vcdSignal_t line, vcdTime_t time, const vcdUartConfig_t *cfg);

/* Writes one frame starting at 'start'. 'flags' is 0 or VCD_UART_FLAG_* bits. 'end' (may be NULL)
 * receives the end of the stop bits, where the next frame may start. */
vcdStatus_t VCD_UartWriteFrame(
    vcdWriter_t *writer,
    vcdSignal_t line,
    vcdTime_t start,
    uint16_t data,
    const vcdUartConfig_t *cfg,
    uint32_t flags,
    vcdTime_t *end);

/* Writes 'count' bytes back to back, with 'gapTicks' of idle line between frames. */
vcdStatus_t VCD_UartWriteBytes(
    vcdWriter_t *writer,
    vcdSignal_t line,
    vcdTime_t start,
    const uint8_t *data,
    size_t count,
    vcdTime_t gapTicks,
    const vcdUartConfig_t *cfg,
    vcdTime_t *end);

/* Holds the line at space level for 'duration' ticks (a break), then returns it to idle. */
vcdStatus_t VCD_UartWriteBreak(
    vcdWriter_t *writer,
    vcdSignal_t line,
    vcdTime_t start,
    vcdTime_t duration,
    const vcdUartConfig_t *cfg);

/* Decodes all frames on a 1-bit variable. cfg->bitTicks is in the reader's timescale
 * (VCD_UartBitTicks(VCD_ReaderTimescale(reader), baud)); cfg->dataSignal is ignored. */
vcdStatus_t VCD_UartDecode(
    const vcdReader_t *reader,
    size_t var,
    const vcdUartConfig_t *cfg,
    vcdUartFrameFn_t onFrame,
    void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* VCD_UART_H */
