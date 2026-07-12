/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "l2_kwp2000.h"
#include "libobd2.h"
#include "libobd2_timer_port.h"
#include <stdio.h>
#include "statusRetCodes.h"
#include "uart_if.h"
#include "libobd2_uart_port.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
static uint8_t currentMode;
static uint16_t bitbangedByte;
static uint16_t bitbangIdx;
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void pushEvent(void* handle, uint8_t byte)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;

    uint32_t *tail = &ctx->tail;
    uint32_t timeNow = ctx->tmrHandle->timeNow;

    ctx->uartEventQueue[*tail].byte = byte;
    ctx->uartEventQueue[*tail].timestamp = timeNow;

    *tail += 1;
    *tail %= ctx->queueSize;
}

static void popEvent(void* handle, uint8_t *byte)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    uint32_t *head = &ctx->head;

    *byte = ctx->uartEventQueue[*head].byte;

    *head += 1;
    *head %= ctx->queueSize;
}

static bool isByteReady(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    uint32_t *head = &ctx->head;
    uint32_t timeNow = ctx->tmrHandle->timeNow;

    return (timeNow > ctx->uartEventQueue[*head].timestamp);
}

/* ================================================ MODULE API ============================================= */
bool LIBOBD2_UART_Init(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    memset(ctx, 0, sizeof(virtUartLinkCtx_t));
    currentMode = DIGITAL_MODE_BEGIN;
    return 1;
}

void LIBOBD2_UART_WriteByte(void* handle, uint8_t data)
{
    pushEvent(handle, data);
}

bool LIBOBD2_UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    if (isByteReady(handle))
    {
        popEvent(handle, recv_buffer);
        return 1;
    }

    return 0;
}

void LIBOBD2_UART_FlushRx(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;

    // TODO: Try to simulate this case. It might be needed only
    // when 5Baud init is in progress because then maybe the KLINE
    // is not held high/ low or the ecu is not open-colector so the
    // echo happens.
    // NOTE: needs more investigating, for now its NOT IMPLEMENTED
}

void LIBOBD2_UART_SendPulse(void* handle, bool pulse)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;

    if (currentMode == DIGITAL_MODE_BEGIN)
    {
        bitbangedByte |= (bitbangIdx << pulse);
        bitbangIdx++;
        bitbangIdx %= 10;
    }

    if (currentMode == DIGITAL_MODE_END)
    {
        bitbangedByte &= 0b0111111110;
        bitbangedByte >>= 1;
        pushEvent(handle, bitbangedByte);
    }
}

void LIBOBD2_UART_SwitchMode(void* handle, uint8_t mode)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    currentMode = mode;
}
