/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "projdefs.h"
#include "libobd2_timer_port.h"
#include <stddef.h>
#define STM32F4
#include "libopencm3/cm3/cortex.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/stm32/f4/nvic.h"
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/f4/timer.h"
#include "statusRetCodes.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"

/* ================================================= MACROS ================================================ */
/*
 * Use TIM2 as a free-running 32-bit microsecond counter.
 *
 * For STM32F401 @ 84MHz:
 *   - APB1 bus = 42MHz (max)
 *   - APB1 timer clock = 84MHz (doubled when APB1 prescaler > 1)
 *   - Prescaler for 1MHz = 84 - 1 = 83
 *
 * TIM2 is 32-bit, so it can count up to 4294 seconds (~71 minutes) before wrapping.
 */
#define TIM2_PRESCALER      83U

/* ============================================ LOCAL VARIABLES ============================================ */
static volatile timerCtx_t *g_timer_ctx = NULL;  /* For ISR access */
bool isTimerStopped = 0;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static inline uint32_t get_time_us(timerCtx_t*);
void KWP_TMR_Pause(void);
void KWP_TMR_Resume(void);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */

/**
 * @brief Get current time in microseconds directly from hardware
 * @return Current time in microseconds (wraps at ~71 minutes)
 */
static inline uint32_t get_time_us(timerCtx_t* ctx)
{
    return timer_get_counter(ctx->timer);
}

/**
 * @brief Literally stop the TIM2 hardware counter.
 *        Called from GDB hook-stop – the counter register freezes,
 *        so all timeout calculations see no elapsed time.
 */
void KWP_TMR_Pause(void)
{
    timer_disable_counter(TIM2);
    isTimerStopped = 1;
}

/**
 * @brief Restart the TIM2 hardware counter.
 *        Called from GDB hook-run – timing resumes from where it left off.
 */
void KWP_TMR_Resume(void)
{
    timer_enable_counter(TIM2);
    isTimerStopped = 0;
}

/* ================================================ MODULE API ============================================= */
bool LIBOBD2_TMR_Init(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    /* Store context */
    g_timer_ctx = ctx;

    /* If the counter is already running (e.g. a second context sharing the
     * same TIM2 32-bit clock), just reset software state and return – do
     * NOT touch the hardware or the free-running counter will be lost. */
    if (TIM_CR1(ctx->timer) & TIM_CR1_CEN)
    {
        ctx->timeout_active  = false;
        ctx->timeout_expired = false;
        return 1;
    }

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
    timer_set_period(ctx->timer, 0xFFFFFFFF);

    /* Generate update event to load prescaler immediately */
    timer_generate_event(ctx->timer, ctx->event);

    /* Clear the update flag that was set by the UG event */
    timer_clear_flag(ctx->timer, ctx->flag);

    /* Reset counter to 0 */
    timer_set_counter(ctx->timer, 0);

    /* Start the counter */
    timer_enable_counter(ctx->timer);

    return 1;
}

uint32_t LIBOBD2_TMR_GetTimeMs(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    return get_time_us(ctx) / 1000U;
}

void LIBOBD2_TMR_DelayMs(void *pHandle, uint32_t delay_ms)
{
    (void)pHandle;
#if 1 // Add special handling for testing
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    uint32_t start_us = get_time_us(ctx);
    uint32_t delay_us = delay_ms * 1000U;

    /* Yield to scheduler while waiting - gives true ms precision from
     * hardware timer (TIM2) without depending on configTICK_RATE_HZ */
    while ((get_time_us(ctx) - start_us) < delay_us);
#else

    /* Block this task and yield CPU to the other task.
     * vTaskDelay suspends the caller for the requested number of ticks,
     * allowing the scheduler to run the peer task freely during the wait.
     * This replaces the previous busy-wait which starved the other task
     * for the full duration of every P2/P3/P4 delay. */
    vTaskDelay(pdMS_TO_TICKS(delay_ms + 1));
#endif
}

bool LIBOBD2_TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    cm_disable_interrupts();

    ctx->timeout_callback = callback;
    ctx->timeout_user_data = pUserData;
    ctx->timeout_start_ms = get_time_us(ctx) / 1000U;
    ctx->timeout_duration_ms = timeout_ms;
    ctx->timeout_expired = false;
    ctx->timeout_active = true;

    cm_enable_interrupts();

    return 1;
}

bool LIBOBD2_TMR_StopTimeout(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    ctx->timeout_active = false;

    return 1;
}

bool LIBOBD2_TMR_IsTimeoutExpired(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    if (ctx->timeout_active) {
        uint32_t now_ms = get_time_us(ctx) / 1000U;
        uint32_t elapsed = now_ms - ctx->timeout_start_ms;

        if (elapsed >= ctx->timeout_duration_ms) {
            ctx->timeout_active = false;
            ctx->timeout_expired = true;

            if (ctx->timeout_callback != NULL) {
                ctx->timeout_callback(ctx->timeout_user_data);
            }
        }
    }

    return ctx->timeout_expired;
}
