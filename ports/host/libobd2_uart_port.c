/** NOTE: This file location and name, may implies, that the API provided is a port on the x86 systems and similar.
 * But the intended usage is to provided a virtual port, to emulate the communication between tester and ecu, in a virtual environment.
 * TODO: Therefore a more suitable name and place shall be found */
/* ================================================ INCLUDES =============================================== */
#include "libobd2_uart_port.h"
#include "libobd2_timer_port.h"
#include "uart_if.h"
#include "uart_trace.h"
#include "utils.h"
#include "cQueue.h"
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static inline bool UART_IsByteReady(const virtUartLinkCtx_t *ctx);
static inline void UART_AdvanceTime(virtUartLinkCtx_t *ctx);
static void UART_PutOnLine(virtUartLinkCtx_t *ctx, uartEvent_t *event);

static bool UART_Init_Validate(const virtUartLinkCtx_t *ctx);
static bool UART_Init_Execute(virtUartLinkCtx_t *ctx);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: UART_IsByteReady
* brief: True when the oldest byte on the line was written before the current
*        virtual time, so the receiver can see it.
******************************************/
static inline bool UART_IsByteReady(const virtUartLinkCtx_t *ctx)
{
    uartEvent_t event = {0};
    bool ready = false;

    // TODO: can add random small delay for simulating cpu clk cycles
    if (q_peek(ctx->uartEventQueue, &event))
    {
        ready = (ctx->tmrHandle->timeNow > event.timeReceived);
    }

    return ready;
}

/******************************************
* title: UART_AdvanceTime
* brief: Moves the virtual clock by one poll step.
******************************************/
static inline void UART_AdvanceTime(virtUartLinkCtx_t *ctx)
{
    // TODO: can add random small delay for simulating cpu clk cycles
    ctx->tmrHandle->timeNow += ctx->tmrHandle->timeStep;
}

/******************************************
* title: UART_PutOnLine
* brief: Puts a byte on the line at the current virtual time. The onWrite
*        fault hook may corrupt it first.
******************************************/
static void UART_PutOnLine(virtUartLinkCtx_t *ctx, uartEvent_t *event)
{
    event->timeReceived = ctx->tmrHandle->timeNow;

    if (NULL != ctx->faults.onWrite)
    {
        ctx->faults.onWrite(ctx->faults.pUserData, event);
    }

    q_push(ctx->uartEventQueue, event);
}

/******************************************
* title: UART_Init_Validate
* brief: Checks the arguments of UART_Init.
******************************************/
static bool UART_Init_Validate(const virtUartLinkCtx_t *ctx)
{
    return (NULL != ctx);
}

/******************************************
* title: UART_Init_Execute
* brief: Gives the link a new empty line (freeing the old one) and resets the
*        line mode and bit-bang state. Fault hooks are kept.
******************************************/
static bool UART_Init_Execute(virtUartLinkCtx_t *ctx)
{
    Queue_t *pQueue = (Queue_t *)calloc(1, sizeof(Queue_t));
    bool ok = false;

    OBD2_IF_COND_GOTO_EXIT(NULL == pQueue);

    if (NULL == q_init(pQueue, sizeof(uartEvent_t), VIRT_UART_QUEUE_SIZE, FIFO, true))
    {
        free(pQueue);
        goto exit;
    }

    if (NULL != ctx->uartEventQueue)
    {
        q_kill(ctx->uartEventQueue);
        free(ctx->uartEventQueue);
    }
    ctx->uartEventQueue = pQueue;

    ctx->mode          = DIGITAL_MODE_END; /* the UART owns the line */
    ctx->bitbangedByte = 0;
    ctx->bitbangIdx    = 0;

    // TODO: can add random small delay for simulating cpu clk cycles
    ok = true;
exit:
    return ok;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: UART_Init
* brief: Opens the virtual K-line for a link context, with an empty line.
******************************************/
bool UART_Init(void* handle)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t *)handle;
    bool ok = false;

    ok = UART_Init_Validate(ctx);
    OBD2_IF_COND_GOTO_EXIT(!ok);
    ok = UART_Init_Execute(ctx);
    OBD2_IF_COND_GOTO_EXIT(!ok);

    // TODO: add a ifdef TRACING - endif /* TRACING */
    UART_TRACE_OnLinkInit();
exit:
    return ok;
}

/******************************************
* title: UART_WriteByte
* brief: Puts a byte on the line at the current virtual time. The onWrite
*        fault hook may corrupt it first.
******************************************/
void UART_WriteByte(void* handle, uint8_t data)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t *)handle;
    uartEvent_t event = {0};

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->tmrHandle) && (NULL != ctx->uartEventQueue));

    event.byte  = data;
    event.error = OBD_UART_RX_OK;
    UART_PutOnLine(ctx, &event);

    UART_TRACE_OnWrite(event.byte, ctx->tmrHandle->timeNow, (uint8_t)event.error);
}

/******************************************
* title: UART_RecvByte
* brief: Takes the oldest byte off the line if it is visible yet, then moves
*        the clock one step. The onRead fault hook may set its error.
******************************************/
obd_uart_rx_status_t UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t *)handle;
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_NO_DATA;
    uartEvent_t event = {0};

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->tmrHandle) && (NULL != ctx->uartEventQueue));
    OBD2_DEBUG_ASSERT(NULL != recv_buffer);

    if (UART_IsByteReady(ctx))
    {
        q_pop(ctx->uartEventQueue, &event);

        if (NULL != ctx->faults.onRead)
        {
            ctx->faults.onRead(ctx->faults.pUserData, &event);
        }

        *recv_buffer = event.byte;
        rxStatus     = event.error;
    }

    UART_AdvanceTime(ctx);

    UART_TRACE_OnRead(event.byte, ctx->tmrHandle->timeNow, (OBD_UART_RX_NO_DATA != rxStatus));

    return rxStatus;
}

/******************************************
* title: UART_FlushRx
* brief: Not implemented: the virtual line has no echo to drop yet.
******************************************/
void UART_FlushRx(void* handle)
{
    // TODO: Try to simulate this case. It might be needed only
    // when 5Baud init is in progress because then maybe the KLINE
    // is not held high/ low or the ecu is not open-colector so the
    // echo happens.
    (void)handle;
}

/******************************************
* title: UART_SendPulse
* brief: Records one bit-banged bit (start, data LSB first, stop) while the
*        line is in DIGITAL_MODE_BEGIN; other modes ignore it.
******************************************/
void UART_SendPulse(void* handle, bool pulse)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t *)handle;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->tmrHandle));

    UART_TRACE_OnPulse(pulse, ctx->tmrHandle->timeNow);

    if ((DIGITAL_MODE_BEGIN == ctx->mode) && (ctx->bitbangIdx < VIRT_UART_BITBANG_FRAME_BITS))
    {
        ctx->bitbangedByte |= (uint16_t)((uint16_t)pulse << ctx->bitbangIdx);
        ctx->bitbangIdx++;
    }
}

/******************************************
* title: UART_SwitchMode
* brief: Sets the line mode. DIGITAL_MODE_BEGIN starts a new bit-banged byte;
*        DIGITAL_MODE_END puts the complete byte on the line.
******************************************/
void UART_SwitchMode(void* handle, uint8_t mode)
{
    virtUartLinkCtx_t *ctx = (virtUartLinkCtx_t *)handle;
    uartEvent_t event = {0};

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->tmrHandle) && (NULL != ctx->uartEventQueue));

    if (DIGITAL_MODE_BEGIN == mode)
    {
        ctx->bitbangedByte = 0;
        ctx->bitbangIdx    = 0;
    }

    if ((DIGITAL_MODE_END == mode) && (DIGITAL_MODE_BEGIN == ctx->mode)
     && (VIRT_UART_BITBANG_FRAME_BITS == ctx->bitbangIdx))
    {
        event.byte  = (uint8_t)((ctx->bitbangedByte >> VIRT_UART_BITBANG_DATA_SHIFT) & VIRT_UART_BITBANG_DATA_MASK);
        event.error = OBD_UART_RX_OK;
        UART_PutOnLine(ctx, &event);
    }

    ctx->mode = mode;
    UART_TRACE_OnMode(mode, ctx->tmrHandle->timeNow);
}
