/* ================================================ INCLUDES =============================================== */
#include "libobd2_timer_port.h"
#include "uart_trace.h"
#include "utils.h"
#include <stddef.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static inline void TMR_InjectFault(virtTmrCtx_t *ctx, virtTmrEvent_t *event);

static bool TMR_Init_Validate(const virtTmrCtx_t *ctx);
static void TMR_Init_Execute(virtTmrCtx_t *ctx);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: TMR_InjectFault
* brief: Hands an operation to the onOp fault hook, if one is set, so it can
*        change the value used or the status/expired result returned.
******************************************/
static inline void TMR_InjectFault(virtTmrCtx_t *ctx, virtTmrEvent_t *event)
{
    if (NULL != ctx->faults.onOp)
    {
        ctx->faults.onOp(ctx->faults.pUserData, event);
    }
}

/******************************************
* title: TMR_Init_Validate
* brief: Checks the arguments of TMR_Init.
******************************************/
static bool TMR_Init_Validate(const virtTmrCtx_t *ctx)
{
    return (NULL != ctx);
}

/******************************************
* title: TMR_Init_Execute
* brief: Clears time, step and timeout. Fault hooks are kept.
******************************************/
static void TMR_Init_Execute(virtTmrCtx_t *ctx)
{
    const virtTmrFaultHooks_t faults = ctx->faults;

    memset(ctx, 0, sizeof(virtTmrCtx_t));
    ctx->faults = faults;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: TMR_Init
* brief: Puts the virtual clock back to time 0 with no timeout running,
*        unless the onOp fault hook makes it fail.
******************************************/
obd_tmr_status_t TMR_Init(void *pHandle)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t *)pHandle;
    virtTmrEvent_t event = { .op = VIRT_TMR_OP_INIT, .status = OBD_TMR_OK };

    event.status = TMR_Init_Validate(ctx) ? OBD_TMR_OK : OBD_TMR_ERR_INVALID_ARG;
    OBD2_IF_COND_GOTO_EXIT(OBD_TMR_OK != event.status);

    TMR_InjectFault(ctx, &event);
    OBD2_IF_COND_GOTO_EXIT(OBD_TMR_OK != event.status);

    TMR_Init_Execute(ctx);

exit:
    return event.status;
}

/******************************************
* title: TMR_GetTimeMs
* brief: Returns the virtual time without moving the clock.
******************************************/
uint32_t TMR_GetTimeMs(void *pHandle)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t *)pHandle;
    virtTmrEvent_t event = { .op = VIRT_TMR_OP_GET_TIME };

    OBD2_DEBUG_ASSERT(NULL != ctx);

    event.value = ctx->timeNow;
    TMR_InjectFault(ctx, &event);

    return event.value;
}

/******************************************
* title: TMR_DelayMs
* brief: Moves the virtual clock forward by delay_ms.
******************************************/
void TMR_DelayMs(void *pHandle, uint32_t delay_ms)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t *)pHandle;
    virtTmrEvent_t event = { .op = VIRT_TMR_OP_DELAY };

    OBD2_DEBUG_ASSERT(NULL != ctx);

    event.value = delay_ms;
    TMR_InjectFault(ctx, &event);

    UART_TRACE_OnDelay(ctx->timeNow, event.value);
    ctx->timeNow += event.value;
}

/******************************************
* title: TMR_StartTimeout
* brief: Arms the timeout timeout_ms after the current time. The callback is
*        not supported; poll TMR_IsTimeoutExpired instead.
******************************************/
obd_tmr_status_t TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t *)pHandle;
    virtTmrEvent_t event = { .op = VIRT_TMR_OP_START_TIMEOUT, .status = OBD_TMR_OK };

    OBD2_DEBUG_ASSERT(NULL != ctx);
    (void)callback;
    (void)pUserData;

    event.value = timeout_ms;
    TMR_InjectFault(ctx, &event);

    if (OBD_TMR_OK == event.status)
    {
        ctx->timeoutTime    = ctx->timeNow + event.value;
        ctx->timeoutStarted = true;
    }

    return event.status;
}

/******************************************
* title: TMR_StopTimeout
* brief: Disarms the timeout.
******************************************/
obd_tmr_status_t TMR_StopTimeout(void *pHandle)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t *)pHandle;
    virtTmrEvent_t event = { .op = VIRT_TMR_OP_STOP_TIMEOUT, .status = OBD_TMR_OK };

    OBD2_DEBUG_ASSERT(NULL != ctx);

    TMR_InjectFault(ctx, &event);

    if (OBD_TMR_OK == event.status)
    {
        ctx->timeoutStarted = false;
    }

    return event.status;
}

/******************************************
* title: TMR_IsTimeoutExpired
* brief: True once the virtual time is past the deadline of a started
*        timeout; a stopped timeout never reports expired.
******************************************/
bool TMR_IsTimeoutExpired(void *pHandle)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t *)pHandle;
    virtTmrEvent_t event = { .op = VIRT_TMR_OP_IS_TIMEOUT_EXPIRED };

    OBD2_DEBUG_ASSERT(NULL != ctx);

    event.expired = ctx->timeoutStarted && (ctx->timeoutTime < ctx->timeNow);
    TMR_InjectFault(ctx, &event);

    return event.expired;
}
