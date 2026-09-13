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
/******************************************************************
 * Descriptions: Pushes uart events in a queue
 *   1. State 1: uart event queue
 *   ================================================================
 *   | 0   1  2 3 4 5 6 7 8 9 <- queue size
 *   | e1  e2 e3
 *   | ^      ^
 *   | head   tail
 *   ================================================================
 *
 *   2. State 2: new event comes, need to push. Tail will increment with 1
 *   ================================================================
 *   | 0     1  2  3 4 5 6 7 8 9 <- queue size
 *   | e1    e2 e3 e4
 *   | ^           ^
 *   | head        tail
 *   ================================================================
 * Input: byte to be pushed
 * Output: Byte to be pushed will be saved in the queue at the current time
 ******************************************************************/
static inline void pushEvent(void* handle, uint8_t byte)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;

    uint32_t *tail = &ctx->tail;
    uint32_t timeNow = ctx->tmrHandle->timeNow;

    ctx->uartEventQueue[*tail].byte = byte;
    ctx->uartEventQueue[*tail].timeReceived = timeNow;

    *tail += 1;
    *tail %= ctx->queueSize;

    // TODO: can add random small delay for simulating cpu clk cycles
}

/******************************************************************
 * Descriptions: Pops uart events from the queue
 *   1. State 1: uart event queue
 *   ================================================================
 *   | 0   1  2 3 4 5 6 7 8 9 <- queue size
 *   | e1  e2 e3
 *   | ^      ^
 *   | head   tail
 *   ================================================================
 *
 *   2. State 2: Pop whats on the head and move one index right
 *   ================================================================
 *   | 0  1    2  3 4 5 6 7 8 9 <- queue size
 *   | e1 e2   e3 e4
 *   |    ^       ^
 *   |    head    tail
 *   ================================================================
 * Input: Nothing
 * Output: The byte at the current head index.
 ******************************************************************/
static inline void popEvent(void* handle, uint8_t *byte)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    uint32_t *head = &ctx->head;

    *byte = ctx->uartEventQueue[*head].byte;

    *head += 1;
    *head %= ctx->queueSize;

    // TODO: can add random small delay for simulating cpu clk cycles
}

static inline bool byteReceived(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    uint32_t *head = &ctx->head;
    uint32_t timeNow = ctx->tmrHandle->timeNow;

    // TODO: can add random small delay for simulating cpu clk cycles

    // if we received a byte in the past
    return (timeNow > ctx->uartEventQueue[*head].timeReceived);
}

static inline void incTime(void *handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    ctx->tmrHandle->timeNow += ctx->tmrHandle->timeStep;

    // TODO: can add random small delay for simulating cpu clk cycles
}

/* ================================================ MODULE API ============================================= */
bool LIBOBD2_UART_Init(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    memset(ctx, 0, sizeof(virtUartLinkCtx_t));
    currentMode   = DIGITAL_MODE_BEGIN;
    bitbangedByte = 0;
    bitbangIdx    = 0;

    // TODO: can add random small delay for simulating cpu clk cycles
    return 1;
}

void LIBOBD2_UART_WriteByte(void* handle, uint8_t data)
{
    pushEvent(handle, data);
}

bool LIBOBD2_UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    bool status = 0;

    if (byteReceived(handle))
    {
        // ack message
        popEvent(handle, recv_buffer);
        status = 1;
    }

    // advance time
    incTime(handle);

    return status;
}

void LIBOBD2_UART_FlushRx(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;

    // TODO: Try to simulate this case. It might be needed only
    // when 5Baud init is in progress because then maybe the KLINE
    // is not held high/ low or the ecu is not open-colector so the
    // echo happens.
    // NOTE: needs more investigating, for now its NOT IMPLEMENTED

    // NOTE: Proposed implementation
    // uint8_t* byte;
    // popEvent(handle, byte);
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
