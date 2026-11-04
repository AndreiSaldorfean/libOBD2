/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "projdefs.h"
#include "libobd2_timer_port.h"
#include <stddef.h>
#include "libopencm3/cm3/cortex.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/stm32/g4/nvic.h"
#include "libopencm3/stm32/g4/rcc.h"
#include "libopencm3/stm32/g4/timer.h"
#include "statusRetCodes.h"
#include "utils.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"

/* ================================================= MACROS ================================================ */
/*
 * The timer runs as a free-running 32-bit microsecond counter: ctx->timerPrescaler
 * divides the timer clock down to 1 MHz (e.g. 170 MHz -> 169). A 32-bit counter
 * wraps after ~71 minutes.
 */
#define TMR_US_PER_MS     (1000U)
#define TMR_COUNTER_MAX   (0xFFFFFFFFU)

/* ============================================ LOCAL VARIABLES ============================================ */
bool isTimerStopped = 0;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static inline uint32_t TMR_GetTimeUs(const timerCtx_t *ctx);

static bool TMR_Init_Validate(const timerCtx_t *ctx);
static void TMR_Init_Execute(timerCtx_t *ctx);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: TMR_GetTimeUs
* brief: Current time in microseconds, read straight from the counter
*        (wraps after ~71 minutes).
******************************************/
static inline uint32_t TMR_GetTimeUs(const timerCtx_t *ctx)
{
    return timer_get_counter(ctx->timer);
}

/******************************************
* title: TMR_Init_Validate
* brief: Checks the arguments of LIBOBD2_TMR_Init.
******************************************/
static bool TMR_Init_Validate(const timerCtx_t *ctx)
{
    return (NULL != ctx);
}

/******************************************
* title: TMR_Init_Execute
* brief: Clears the timeout state and, unless another context already started
*        it, configures and starts the free-running microsecond counter.
******************************************/
static void TMR_Init_Execute(timerCtx_t *ctx)
{
    ctx->timeout_active  = false;
    ctx->timeout_expired = false;

    /* If the counter is already running (e.g. a second context sharing the
     * same TIM2 32-bit clock), stop here – do NOT touch the hardware or the
     * free-running counter will be lost. */
    OBD2_IF_COND_GOTO_EXIT(0U != (TIM_CR1(ctx->timer) & TIM_CR1_CEN));

    /* Enable TIM2 clock */
    rcc_periph_clock_enable(ctx->timerClk);

    /* Reset TIM2 peripheral to defaults */
    rcc_periph_reset_pulse(ctx->rstTimer);

    /* Disable counter during configuration */
    timer_disable_counter(ctx->timer);

    /*
     * Configure TIM2 as free-running 32-bit microsecond counter.
     * No interrupts needed - just read the counter directly.
     */
    timer_set_prescaler(ctx->timer, ctx->timerPrescaler);

    /* Disable preload for immediate prescaler update */
    timer_disable_preload(ctx->timer);
    timer_continuous_mode(ctx->timer);

    /* Set period to max (32-bit) - free-running counter */
    timer_set_period(ctx->timer, TMR_COUNTER_MAX);

    /* Generate update event to load prescaler immediately */
    timer_generate_event(ctx->timer, ctx->event);

    /* Clear the update flag that was set by the UG event */
    timer_clear_flag(ctx->timer, ctx->flag);

    /* Reset counter to 0 */
    timer_set_counter(ctx->timer, 0);

    /* Start the counter */
    timer_enable_counter(ctx->timer);

exit:
    return;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: KWP_TMR_Pause
* brief: Freezes the TIM2 counter (GDB hook-stop), so no time passes while
*        the target is halted.
******************************************/
void KWP_TMR_Pause(void)
{
    timer_disable_counter(TIM2);
    isTimerStopped = 1;
}

/******************************************
* title: KWP_TMR_Resume
* brief: Restarts the TIM2 counter (GDB hook-run) from where it stopped.
******************************************/
void KWP_TMR_Resume(void)
{
    timer_enable_counter(TIM2);
    isTimerStopped = 0;
}

/******************************************
* title: LIBOBD2_TMR_Init
* brief: Starts the microsecond counter (once) and clears the timeout.
******************************************/
obd_tmr_status_t LIBOBD2_TMR_Init(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t *)pHandle;
    obd_tmr_status_t tmrStatus = OBD_TMR_ERR_INVALID_ARG;

    OBD2_IF_COND_GOTO_EXIT(!TMR_Init_Validate(ctx));
    TMR_Init_Execute(ctx);
    tmrStatus = OBD_TMR_OK;

exit:
    return tmrStatus;
}

/******************************************
* title: LIBOBD2_TMR_GetTimeMs
* brief: Current time in milliseconds.
******************************************/
uint32_t LIBOBD2_TMR_GetTimeMs(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t *)pHandle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    return TMR_GetTimeUs(ctx) / TMR_US_PER_MS;
}

/******************************************
* title: LIBOBD2_TMR_DelayMs
* brief: Blocks the calling task for delay_ms and lets the other tasks run.
******************************************/
void LIBOBD2_TMR_DelayMs(void *pHandle, uint32_t delay_ms)
{
    (void)pHandle;

    /* Block this task and yield CPU to the other task.
     * vTaskDelay suspends the caller for the requested number of ticks,
     * allowing the scheduler to run the peer task freely during the wait.
     * This replaces the previous busy-wait which starved the other task
     * for the full duration of every P2/P3/P4 delay. */
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

/******************************************
* title: LIBOBD2_TMR_StartTimeout
* brief: Arms the timeout timeout_ms from now; callback runs when
*        LIBOBD2_TMR_IsTimeoutExpired sees it expire.
******************************************/
obd_tmr_status_t LIBOBD2_TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData)
{
    timerCtx_t *ctx = (timerCtx_t *)pHandle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    cm_disable_interrupts();

    ctx->timeout_callback    = callback;
    ctx->timeout_user_data   = pUserData;
    ctx->timeout_start_ms    = TMR_GetTimeUs(ctx) / TMR_US_PER_MS;
    ctx->timeout_duration_ms = timeout_ms;
    ctx->timeout_expired     = false;
    ctx->timeout_active      = true;

    cm_enable_interrupts();

    return OBD_TMR_OK;
}

/******************************************
* title: LIBOBD2_TMR_StopTimeout
* brief: Disarms the timeout; a stopped timeout never reports expired.
******************************************/
obd_tmr_status_t LIBOBD2_TMR_StopTimeout(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t *)pHandle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    ctx->timeout_active  = false;
    ctx->timeout_expired = false;

    return OBD_TMR_OK;
}

/******************************************
* title: LIBOBD2_TMR_IsTimeoutExpired
* brief: True once the armed timeout has run out (wrap-around safe); runs the
*        callback the first time it sees it expire.
******************************************/
bool LIBOBD2_TMR_IsTimeoutExpired(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t *)pHandle;
    uint32_t elapsed = 0;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    if (ctx->timeout_active)
    {
        elapsed = (TMR_GetTimeUs(ctx) / TMR_US_PER_MS) - ctx->timeout_start_ms;

        if (elapsed >= ctx->timeout_duration_ms)
        {
            ctx->timeout_active  = false;
            ctx->timeout_expired = true;

            if (NULL != ctx->timeout_callback)
            {
                ctx->timeout_callback(ctx->timeout_user_data);
            }
        }
    }

    return ctx->timeout_expired;
}
