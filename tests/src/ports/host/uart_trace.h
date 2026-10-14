#ifndef UART_TRACE_H
#define UART_TRACE_H

/*************************************************************************************************************
 * Optional data tracer for the host virtual UART port (libobd2_uart_port.c).
 *
 * When enabled, every byte that crosses LIBOBD2_UART_WriteByte()/
 * LIBOBD2_UART_RecvByte() is appended to a human-readable log file, so the
 * actual data moving through the write/read API can be inspected after a
 * test run instead of only seeing pass/fail. It is generic: it has no idea
 * which test is running, so it traces whatever byte traffic happens while
 * it is enabled, across any number of tests.
 *
 * Disabled by default, so it has no effect (and no runtime cost beyond a
 * flag check) unless a test opts in.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */

/* Set to true/false to turn UART write/read tracing on or off at runtime.
 * Defaults to false: no log file is created unless a test enables this. */
extern bool g_uartTraceEnabled;

/* =============================================== MODULE API ============================================== */

/* Sets the path of the log file tracing is written to. Takes effect the
 * next time a byte is traced: any already-open log file is closed first,
 * so switching paths mid-run starts a fresh file. Call before enabling
 * tracing (or before the next traced byte) to pick where it is written;
 * if never called, UART_TRACE_DEFAULT_LOG_PATH is used. */
void UART_TRACE_SetLogPath(const char *path);

/* Called from LIBOBD2_UART_WriteByte()'s implementation. No-op unless
 * g_uartTraceEnabled is true. */
void UART_TRACE_LogWrite(uint8_t byte, uint32_t timeNow);

/* Called from LIBOBD2_UART_RecvByte()'s implementation, with the resulting
 * byte/success flag. No-op unless g_uartTraceEnabled is true. */
void UART_TRACE_LogRead(uint8_t byte, uint32_t timeNow, bool success);

#endif /* UART_TRACE_H */
