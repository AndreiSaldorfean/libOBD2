/** NOTE: This file location and name, may implies, that the API provided is a port on the x86 systems and similar.
 * But the intended usage is to provided a virtual port, to emulate the communication between tester and ecu, in a virtual environment.
 * TODO: Therefore a more suitable name and place shall be found */
/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "l2_kwp2000.h"
#include "libobd2.h"
#include "libobd2_timer_port.h"
#include <stdio.h>
#include <stdlib.h>
#include "statusRetCodes.h"
#include "uart_if.h"
#include "libobd2_uart_port.h"
#include "uart_trace.h"
#include "cQueue.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
static uint8_t currentMode;
static uint16_t bitbangedByte;
static uint16_t bitbangIdx;
/* ============================================ GLOBAL VARIABLES =========================================== */
static bool s_uart_initialized = false;
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static inline bool byteReceived(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    uint32_t timeNow = ctx->tmrHandle->timeNow;
    uartEvent_t event;

    if (!q_peek(ctx->uartEventQueue, &event))
    {
        return false;
    }

    // TODO: can add random small delay for simulating cpu clk cycles

    // if we received a byte in the past
    return (timeNow > event.timeReceived);
}

static inline void incTime(void *handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    ctx->tmrHandle->timeNow += ctx->tmrHandle->timeStep;

    // TODO: can add random small delay for simulating cpu clk cycles
}
static bool UART_Init_validate(virtUartLinkCtx_t *ctx, Queue_t *pQueue)
{
    if (ctx == NULL)
    {
        return 0;
    }
    if (pQueue == NULL)
    {
        return 0;
    }
    if (s_uart_initialized)
    {
        return 1;
    }

    return 1;
}

static bool UART_Init_execute(virtUartLinkCtx_t *ctx, Queue_t *pQueue)
{
    if (q_init(
        /* Queue_t */    pQueue,               /* pointer to the queue struct */
        /* size_t */     sizeof(uartEvent_t),  /* size of a record in the queue */
        /* uint16_t */   128,                  /* number of records in the queue */
        /* QueueType */  FIFO,                 /* queue implementation type: FIFO, LIFO */
        /* overwite */   true                  /* overwrite previous records when queue is full if set to true */
    ) == NULL)
    {
        free(pQueue);
        return 0;
    }

    if (ctx->uartEventQueue != NULL)
    {
        q_kill(ctx->uartEventQueue);
        free(ctx->uartEventQueue);
    }
    ctx->uartEventQueue = pQueue;

    currentMode   = DIGITAL_MODE_BEGIN;
    bitbangedByte = 0;
    bitbangIdx    = 0;
    s_uart_initialized = 1;

    // TODO: can add random small delay for simulating cpu clk cycles

    return 1;
}

/* ================================================ MODULE API ============================================= */
bool LIBOBD2_UART_Init(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    Queue_t *pQueue = (Queue_t *)calloc(1, sizeof(Queue_t));

    if (!UART_Init_validate(ctx, pQueue)) { return 0; }
    if (!UART_Init_execute(ctx, pQueue)) { return 0; }
    // TODO: add some verification

    // Other operations
    // TODO: add a ifdef TRACING - endif /* TRACING */
    UART_TRACE_OnLinkInit();

    return 1;
}

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
void LIBOBD2_UART_WriteByte(void* handle, uint8_t data)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    uartEvent_t event =
    {
        .byte = data,
        .timeReceived = ctx->tmrHandle->timeNow,
        .error = UART_NO_ERROR
    };

    /* simualte uart erorrs if callback set */
    if(NULL != ctx->erorr_generator)
    {
        ctx->erorr_generator(&event);
    }

    q_push(ctx->uartEventQueue, &event);

    UART_TRACE_OnWrite(event.byte, ctx->tmrHandle->timeNow, (uint8_t)event.error);
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
bool LIBOBD2_UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    uartEvent_t event = {0};
    bool status = 0;

    /* simualte uart erorrs if callback set */
    if(NULL != ctx->erorr_generator)
    {
        ctx->erorr_generator(&event);
    }

    if (byteReceived(handle))
    {
        // ack message
        q_pop(ctx->uartEventQueue, &event);
        status = 1;
    }

    // advance time
    incTime(handle);

    *recv_buffer = event.byte;

    UART_TRACE_OnRead(*recv_buffer, ctx->tmrHandle->timeNow, status);

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

    UART_TRACE_OnPulse(pulse, ctx->tmrHandle->timeNow);

    if (currentMode == DIGITAL_MODE_BEGIN)
    {
        bitbangedByte |= (bitbangIdx << pulse);
        bitbangIdx++;
        bitbangIdx %= 10;
    }

    if (currentMode == DIGITAL_MODE_END)
    {
        uartEvent_t event = {0};
        bitbangedByte &= 0b0111111110;
        bitbangedByte >>= 1;
        event.byte = bitbangedByte;
        q_push(ctx->uartEventQueue, &event);
    }
}

void LIBOBD2_UART_SwitchMode(void* handle, uint8_t mode)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t*)handle;
    currentMode = mode;
    UART_TRACE_OnMode(mode, ctx->tmrHandle->timeNow);
}
