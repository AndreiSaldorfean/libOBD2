#ifndef UART_TRACE_H
#define UART_TRACE_H

/*************************************************************************************************************
 * Waveform tracer for the host virtual UART port (libobd2_uart_port.c), built on VCDio.
 *
 * Between UART_TRACE_Begin() and UART_TRACE_End() everything that happens on the virtual K-line is written
 * to a .vcd file. Only 1-bit signals are used, so PulseView/sigrok import the whole trace:
 *
 *   kline.line        the wire: every written byte as a UART frame (UART_TRACE_BAUD, 8N1) plus the
 *                     5-baud bit-banged pulses
 *   kline.rx_data     every byte LIBOBD2_UART_RecvByte() returned, as a short UART frame
 *                     (UART_TRACE_RX_BAUD, 8N1) at the time of the read
 *   kline.rx_waiting  1 while reads come back empty, 0 once a byte is read
 *   kline.session     short pulse at each LIBOBD2_UART_Init(), i.e. at every PBT trial
 *   kline.uart_error  high during a frame that carries an error from erorr_generator
 *
 * PulseView: Import -> Value Change Dump, then add two UART decoders:
 *   RX = line,    baud UART_TRACE_BAUD     (what was on the wire)
 *   RX = rx_data, baud UART_TRACE_RX_BAUD  (what the library read)
 *
 * Build with -DUART_TRACE_EXTRA_SIGNALS=1 to also get tx_byte/rx_byte vectors, a trial counter and text
 * notes (errors, mode switches, skipped time) for GTKWave/Surfer. Older sigrok versions (0.5.x) stop
 * importing at the first multi-bit value, so keep it off for PulseView.
 *
 * Time: 1 virtual timer tick (ms) = 1 ms of trace time. Each session starts after the previous one, and
 * idle gaps longer than UART_TRACE_MAX_GAP_MS are shortened, so random timer values from property tests
 * still give a compact trace.
 *
 * Disabled until UART_TRACE_Begin(): the hooks cost one pointer check otherwise.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define UART_TRACE_BAUD        10400u  /* K-line baud rate drawn on kline.line */
#define UART_TRACE_RX_BAUD     250000u /* fast frames on kline.rx_data, so each read fits its slot */
#define UART_TRACE_MAX_GAP_MS  50u     /* idle time longer than this is shortened to it */

#ifndef UART_TRACE_EXTRA_SIGNALS
#define UART_TRACE_EXTRA_SIGNALS 0     /* 1: add vector/string signals for GTKWave/Surfer */
#endif

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

/* Starts tracing into 'path' (a .vcd file). A trace already running is finished first.
 * Returns false if the file cannot be created; tracing then stays off. */
bool UART_TRACE_Begin(const char *path);

/* Finishes the running trace and closes its file. No-op when nothing is traced. */
void UART_TRACE_End(void);

/* Shows 'text' on kline.note (only with UART_TRACE_EXTRA_SIGNALS). */
void UART_TRACE_Note(const char *text);

/* Hooks called by the host UART port. */
void UART_TRACE_OnLinkInit(void);
void UART_TRACE_OnWrite(uint8_t byte, uint32_t timeMs, uint8_t error);
void UART_TRACE_OnRead(uint8_t byte, uint32_t timeMs, bool received);
void UART_TRACE_OnPulse(bool level, uint32_t timeMs);
void UART_TRACE_OnMode(uint8_t mode, uint32_t timeMs);

#endif /* UART_TRACE_H */
