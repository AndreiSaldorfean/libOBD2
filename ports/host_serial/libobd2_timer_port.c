/* ================================================ INCLUDES =============================================== */
#define _DEFAULT_SOURCE
#include "libobd2_timer_port.h"
#include "utils.h"
#include <stddef.h>
#include <errno.h>
#include <time.h>

/* ================================================= MACROS ================================================ */
#define TMR_MS_PER_S   (1000U)
#define TMR_NS_PER_MS  (1000000L)

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static inline uint64_t TMR_MonotonicMs(void);

static bool TMR_Init_Validate(const serialTmrCtx_t *ctx);
static void TMR_Init_Execute(serialTmrCtx_t *ctx);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: TMR_MonotonicMs
* brief: Milliseconds of CLOCK_MONOTONIC (not affected by wall-clock changes).
******************************************/
static inline uint64_t TMR_MonotonicMs(void)
{
    struct timespec now = {0};

    (void)clock_gettime(CLOCK_MONOTONIC, &now);

    return ((uint64_t)now.tv_sec * TMR_MS_PER_S) + (uint64_t)(now.tv_nsec / TMR_NS_PER_MS);
}

/******************************************
* title: TMR_Init_Validate
* brief: Checks the arguments of TMR_Init.
******************************************/
static bool TMR_Init_Validate(const serialTmrCtx_t *ctx)
{
    return (NULL != ctx);
}

/******************************************
* title: TMR_Init_Execute
* brief: Restarts the time base at 0 and clears the timeout.
******************************************/
static void TMR_Init_Execute(serialTmrCtx_t *ctx)
{
    ctx->originMs        = TMR_MonotonicMs();
    ctx->timeoutActive   = false;
    ctx->timeoutExpired  = false;
    ctx->timeoutCallback = NULL;
    ctx->timeoutUserData = NULL;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: TMR_Init
* brief: Starts the millisecond time base and clears the timeout.
******************************************/
obd_tmr_status_t TMR_Init(void *pHandle)
{
    serialTmrCtx_t *ctx = (serialTmrCtx_t *)pHandle;
    obd_tmr_status_t tmrStatus = OBD_TMR_ERR_INVALID_ARG;

    OBD2_IF_COND_GOTO_EXIT(!TMR_Init_Validate(ctx));
    TMR_Init_Execute(ctx);
    tmrStatus = OBD_TMR_OK;

exit:
    return tmrStatus;
}

/******************************************
* title: TMR_GetTimeMs
* brief: Milliseconds since TMR_Init (wraps after ~49 days).
******************************************/
uint32_t TMR_GetTimeMs(void *pHandle)
{
    serialTmrCtx_t *ctx = (serialTmrCtx_t *)pHandle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    return (uint32_t)(TMR_MonotonicMs() - ctx->originMs);
}

/******************************************
* title: TMR_DelayMs
* brief: Sleeps until delay_ms from now on the monotonic clock. The absolute
*        deadline keeps the length exact even when a signal interrupts it.
******************************************/
void TMR_DelayMs(void *pHandle, uint32_t delay_ms)
{
    struct timespec deadline = {0};

    (void)pHandle;

    (void)clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec  += (time_t)(delay_ms / TMR_MS_PER_S);
    deadline.tv_nsec += (long)(delay_ms % TMR_MS_PER_S) * TMR_NS_PER_MS;
    if (deadline.tv_nsec >= (TMR_MS_PER_S * TMR_NS_PER_MS))
    {
        deadline.tv_sec  += 1;
        deadline.tv_nsec -= (TMR_MS_PER_S * TMR_NS_PER_MS);
    }

    while (EINTR == clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL))
    {
    }
}

/******************************************
* title: TMR_StartTimeout
* brief: Arms the timeout timeout_ms from now; callback runs when
*        TMR_IsTimeoutExpired sees it expire.
******************************************/
obd_tmr_status_t TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData)
{
    serialTmrCtx_t *ctx = (serialTmrCtx_t *)pHandle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    ctx->timeoutCallback   = callback;
    ctx->timeoutUserData   = pUserData;
    ctx->timeoutStartMs    = TMR_GetTimeMs(ctx);
    ctx->timeoutDurationMs = timeout_ms;
    ctx->timeoutExpired    = false;
    ctx->timeoutActive     = true;

    return OBD_TMR_OK;
}

/******************************************
* title: TMR_StopTimeout
* brief: Disarms the timeout; a stopped timeout never reports expired.
******************************************/
obd_tmr_status_t TMR_StopTimeout(void *pHandle)
{
    serialTmrCtx_t *ctx = (serialTmrCtx_t *)pHandle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    ctx->timeoutActive  = false;
    ctx->timeoutExpired = false;

    return OBD_TMR_OK;
}

/******************************************
* title: TMR_IsTimeoutExpired
* brief: True once the armed timeout has run out (wrap-around safe); runs the
*        callback the first time it sees it expire.
******************************************/
bool TMR_IsTimeoutExpired(void *pHandle)
{
    serialTmrCtx_t *ctx = (serialTmrCtx_t *)pHandle;
    uint32_t elapsed = 0;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    if (ctx->timeoutActive)
    {
        elapsed = timing_elapsed_ms(ctx->timeoutStartMs, TMR_GetTimeMs(ctx));

        if (elapsed >= ctx->timeoutDurationMs)
        {
            ctx->timeoutActive  = false;
            ctx->timeoutExpired = true;

            if (NULL != ctx->timeoutCallback)
            {
                ctx->timeoutCallback(ctx->timeoutUserData);
            }
        }
    }

    return ctx->timeoutExpired;
}
