/* ================================================ INCLUDES =============================================== */
#define _DEFAULT_SOURCE
#include "libobd2_uart_port.h"
#include "libserialport.h"
#include "uart_if.h"
#include "utils.h"
#include <stddef.h>
#include <time.h>
#include <errno.h>
#if defined(__linux__)
#include <sys/ioctl.h>
#include <linux/serial.h>
#endif /* __linux__ */

/* ================================================= MACROS ================================================ */
#define SERIAL_DATA_BITS      (8)
#define SERIAL_STOP_BITS      (1)
#define SERIAL_RX_POLL_MS     (1U)   /* longest UART_RecvByte waits for a byte */
#define SERIAL_TX_TIMEOUT_MS  (100U) /* longest UART_WriteByte waits for room in the OS buffer */
#define SERIAL_NS_PER_MS      (1000000L)

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void UART_SleepMs(uint32_t ms);
static void UART_SetLowLatency(struct sp_port *port);
static void UART_SetBreak(serialUartCtx_t *ctx, bool active);
static bool UART_Configure(serialUartCtx_t *ctx);

static bool UART_Init_Validate(const serialUartCtx_t *ctx);
static bool UART_Init_Execute(serialUartCtx_t *ctx);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: UART_SleepMs
* brief: Sleeps ms milliseconds, resuming after a signal.
******************************************/
static void UART_SleepMs(uint32_t ms)
{
    struct timespec req = { .tv_sec = (time_t)(ms / 1000U), .tv_nsec = (long)(ms % 1000U) * SERIAL_NS_PER_MS };
    struct timespec rem = {0};

    while ((0 != nanosleep(&req, &rem)) && (EINTR == errno))
    {
        req = rem;
    }
}

/******************************************
* title: UART_SetLowLatency
* brief: Asks the driver to hand every received byte over at once. USB
*        adapters (FTDI) otherwise batch bytes for up to 16 ms, which breaks
*        the K-line inter-byte timing checks. Best effort: errors are ignored.
******************************************/
static void UART_SetLowLatency(struct sp_port *port)
{
#if defined(__linux__)
    int fd = -1;
    struct serial_struct serial = {0};

    if ((SP_OK == sp_get_port_handle(port, &fd)) && (0 == ioctl(fd, TIOCGSERIAL, &serial)))
    {
        serial.flags |= ASYNC_LOW_LATENCY;
        (void)ioctl(fd, TIOCSSERIAL, &serial);
    }
#else
    (void)port;
#endif /* __linux__ */
}

/******************************************
* title: UART_SetBreak
* brief: Holds TX low (BREAK) or releases it to idle high, only when the
*        line is not already in that state.
******************************************/
static void UART_SetBreak(serialUartCtx_t *ctx, bool active)
{
    if (active != ctx->breakActive)
    {
        (void)(active ? sp_start_break(ctx->port) : sp_end_break(ctx->port));
        ctx->breakActive = active;
    }
}

/******************************************
* title: UART_Configure
* brief: Sets 8N1 at the configured baud rate with no flow control, and
*        leaves the line idle with both buffers empty.
******************************************/
static bool UART_Configure(serialUartCtx_t *ctx)
{
    const int baud = (0 != ctx->baudRate) ? ctx->baudRate : SERIAL_UART_DEFAULT_BAUD;
    bool ok = false;

    OBD2_IF_COND_GOTO_EXIT(SP_OK != sp_set_baudrate(ctx->port, baud));
    OBD2_IF_COND_GOTO_EXIT(SP_OK != sp_set_bits(ctx->port, SERIAL_DATA_BITS));
    OBD2_IF_COND_GOTO_EXIT(SP_OK != sp_set_parity(ctx->port, SP_PARITY_NONE));
    OBD2_IF_COND_GOTO_EXIT(SP_OK != sp_set_stopbits(ctx->port, SERIAL_STOP_BITS));
    OBD2_IF_COND_GOTO_EXIT(SP_OK != sp_set_flowcontrol(ctx->port, SP_FLOWCONTROL_NONE));

    UART_SetLowLatency(ctx->port);

    ctx->breakActive = true; /* force the release below */
    UART_SetBreak(ctx, false);
    OBD2_IF_COND_GOTO_EXIT(SP_OK != sp_flush(ctx->port, SP_BUF_BOTH));

    ok = true;
exit:
    return ok;
}

/******************************************
* title: UART_Init_Validate
* brief: Checks the arguments of UART_Init.
******************************************/
static bool UART_Init_Validate(const serialUartCtx_t *ctx)
{
    return (NULL != ctx) && (NULL != ctx->portName);
}

/******************************************
* title: UART_Init_Execute
* brief: Opens the port on the first call (later calls reuse it) and
*        configures it. A port that fails to configure is closed again.
******************************************/
static bool UART_Init_Execute(serialUartCtx_t *ctx)
{
    bool ok = false;

    if (NULL == ctx->port)
    {
        OBD2_IF_COND_GOTO_EXIT(SP_OK != sp_get_port_by_name(ctx->portName, &ctx->port));

        if (SP_OK != sp_open(ctx->port, SP_MODE_READ_WRITE))
        {
            UART_Deinit(ctx);
            goto exit;
        }
    }

    ok = UART_Configure(ctx);
    if (!ok)
    {
        UART_Deinit(ctx);
    }

exit:
    return ok;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: UART_Init
* brief: Opens and configures the serial port for K-line communication.
******************************************/
bool UART_Init(void* handle)
{
    serialUartCtx_t *ctx = (serialUartCtx_t *)handle;
    bool ok = false;

    ok = UART_Init_Validate(ctx);
    OBD2_IF_COND_GOTO_EXIT(!ok);
    ok = UART_Init_Execute(ctx);

exit:
    return ok;
}

/******************************************
* title: UART_Deinit
* brief: Closes the port, if it is open.
******************************************/
void UART_Deinit(void* handle)
{
    serialUartCtx_t *ctx = (serialUartCtx_t *)handle;

    OBD2_IF_COND_GOTO_EXIT((NULL == ctx) || (NULL == ctx->port));

    (void)sp_close(ctx->port);
    sp_free_port(ctx->port);
    ctx->port        = NULL;
    ctx->breakActive = false;

exit:
    return;
}

/******************************************
* title: UART_WriteByte
* brief: Sends one byte and waits until the OS has handed it to the adapter.
******************************************/
void UART_WriteByte(void* handle, uint8_t data)
{
    serialUartCtx_t *ctx = (serialUartCtx_t *)handle;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->port));

    (void)sp_blocking_write(ctx->port, &data, 1U, SERIAL_TX_TIMEOUT_MS);
    (void)sp_drain(ctx->port);
}

/******************************************
* title: UART_RecvByte
* brief: Waits up to SERIAL_RX_POLL_MS for one byte. libserialport does not
*        report per-byte line errors, so a received byte is always OK; a read
*        error is reported as no data.
******************************************/
obd_uart_rx_status_t UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    serialUartCtx_t *ctx = (serialUartCtx_t *)handle;
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_NO_DATA;
    uint8_t byte = 0;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->port));
    OBD2_DEBUG_ASSERT(NULL != recv_buffer);

    if (1 == sp_blocking_read(ctx->port, &byte, 1U, SERIAL_RX_POLL_MS))
    {
        *recv_buffer = byte;
        rxStatus     = OBD_UART_RX_OK;
    }

    return rxStatus;
}

/******************************************
* title: UART_FlushRx
* brief: Lets the echo of the bytes just sent come back from the K-line,
*        then drops everything pending on the receive side.
******************************************/
void UART_FlushRx(void* handle)
{
    serialUartCtx_t *ctx = (serialUartCtx_t *)handle;
    uint32_t settleMs = SERIAL_UART_DEFAULT_SETTLE_MS;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->port));

    if (0U != ctx->flushSettleMs)
    {
        settleMs = ctx->flushSettleMs;
    }

    (void)sp_drain(ctx->port);
    UART_SleepMs(settleMs);
    (void)sp_flush(ctx->port, SP_BUF_INPUT);
}

/******************************************
* title: UART_SendPulse
* brief: Drives the line low (BREAK on) or high (BREAK off).
******************************************/
void UART_SendPulse(void* handle, bool pulse)
{
    serialUartCtx_t *ctx = (serialUartCtx_t *)handle;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->port));

    UART_SetBreak(ctx, !pulse);
}

/******************************************
* title: UART_SwitchMode
* brief: *_BEGIN / *_START: waits for TX to finish and idles the line high,
*        ready for UART_SendPulse. *_END: releases the line and drops the
*        echo the wake-up pattern left on the receive side.
******************************************/
void UART_SwitchMode(void* handle, uint8_t mode)
{
    serialUartCtx_t *ctx = (serialUartCtx_t *)handle;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->port));

    switch (mode)
    {
        case FAST_INIT_WAKEUP_START:
        case DIGITAL_MODE_BEGIN:
        {
            (void)sp_drain(ctx->port);
            UART_SetBreak(ctx, false);
            break;
        }
        case FAST_INIT_WAKEUP_END:
        case DIGITAL_MODE_END:
        {
            UART_SetBreak(ctx, false);
            (void)sp_flush(ctx->port, SP_BUF_INPUT);
            break;
        }
        default:
            break;
    }
}
