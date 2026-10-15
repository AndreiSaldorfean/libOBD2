/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "libobd2_timer_port.h"
#include <stddef.h>
#include "statusRetCodes.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
bool LIBOBD2_TMR_Init(void *pHandle)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t*)pHandle;
    memset(ctx, 0, sizeof(virtTmrCtx_t));
    return 1;
}

uint32_t LIBOBD2_TMR_GetTimeMs(void *pHandle)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t*)pHandle;
    return ctx->timeNow;
}

void LIBOBD2_TMR_DelayMs(void *pHandle, uint32_t delay_ms)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t*)pHandle;
    ctx->timeNow += delay_ms;
}

bool LIBOBD2_TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t*)pHandle;
    ctx->timeoutTime = ctx->timeNow + timeout_ms;
    ctx->timeoutStarted = 1;
    return 1;
}

bool LIBOBD2_TMR_StopTimeout(void *pHandle)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t*)pHandle;
    ctx->timeoutStarted = 0;
    return 1;
}

bool LIBOBD2_TMR_IsTimeoutExpired(void *pHandle)
{
    virtTmrCtx_t *ctx = (virtTmrCtx_t*)pHandle;
    return (ctx->timeoutTime < ctx->timeNow);
}
