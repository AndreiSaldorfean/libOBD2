/* ================================================ INCLUDES =============================================== */
#include "uart_trace.h"
#include <stdio.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define UART_TRACE_DEFAULT_LOG_PATH "uart_trace_log.txt"
#define UART_TRACE_PATH_MAX         256u

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void writeHeader(void);
static void ensureOpen(void);
static void logEvent(const char *op, uint8_t byte, uint32_t timeNow, const char *result);

/* ============================================ LOCAL VARIABLES ============================================ */
static char     g_uartTracePath[UART_TRACE_PATH_MAX] = UART_TRACE_DEFAULT_LOG_PATH;
static FILE    *g_uartTraceFile = NULL;
static uint32_t g_uartTraceSeq  = 0u;

/* ============================================ GLOBAL VARIABLES =========================================== */
bool g_uartTraceEnabled = false;

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void writeHeader(void)
{
    fprintf(g_uartTraceFile, "================================================================================\n");
    fprintf(g_uartTraceFile, " UART write/read data trace\n");
    fprintf(g_uartTraceFile, "================================================================================\n");
    fprintf(g_uartTraceFile, " %6s | %-5s | %-10s | %10s | %-6s\n",
            "Seq", "Op", "Byte", "Time", "Result");
    fprintf(g_uartTraceFile, "--------+-------+------------+------------+--------\n");
}

/* Lazily (re)opens the log file the first time a byte is traced after
 * (re)configuration, so a test that never enables tracing never creates
 * a file at all. */
static void ensureOpen(void)
{
    if (g_uartTraceFile != NULL) { return; }

    g_uartTraceFile = fopen(g_uartTracePath, "w");
    g_uartTraceSeq  = 0u;

    if (g_uartTraceFile == NULL) { return; }

    writeHeader();
}

static void logEvent(const char *op, uint8_t byte, uint32_t timeNow, const char *result)
{
    if (!g_uartTraceEnabled) { return; }

    ensureOpen();
    if (g_uartTraceFile == NULL) { return; }

    g_uartTraceSeq++;

    fprintf(g_uartTraceFile, " %6u | %-5s | 0x%02X (%3u) | %10u | %-6s\n",
            g_uartTraceSeq, op, byte, byte, timeNow, result);
    fflush(g_uartTraceFile);
}

/* ================================================ MODULE API ============================================= */
void UART_TRACE_SetLogPath(const char *path)
{
    if (path == NULL) { return; }

    if (g_uartTraceFile != NULL)
    {
        fclose(g_uartTraceFile);
        g_uartTraceFile = NULL;
    }

    strncpy(g_uartTracePath, path, sizeof(g_uartTracePath) - 1u);
    g_uartTracePath[sizeof(g_uartTracePath) - 1u] = '\0';
}

void UART_TRACE_LogWrite(uint8_t byte, uint32_t timeNow)
{
    logEvent("WRITE", byte, timeNow, "-");
}

void UART_TRACE_LogRead(uint8_t byte, uint32_t timeNow, bool success)
{
    logEvent("READ", byte, timeNow, success ? "OK" : "EMPTY");
}
