/* ================================================ INCLUDES =============================================== */
#include "kwp_timer.h"
#include <stddef.h>
#define STM32F4
#include "libopencm3/cm3/cortex.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/stm32/f4/nvic.h"
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/f4/timer.h"
#include "srv_status.h"

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

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static inline uint32_t get_time_us(void);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */

/**
 * @brief Get current time in microseconds directly from hardware
 * @return Current time in microseconds (wraps at ~71 minutes)
 */
static inline uint32_t get_time_us(void)
{
    return timer_get_counter(TIM2);
}

/* ================================================ MODULE API ============================================= */
obd_status_t KWP_TMR_Init(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    /* Store context */
    g_timer_ctx = ctx;

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

    return OBD_STATUS_OK;
}

uint32_t KWP_TMR_GetTimeMs(void *pHandle)
{
    (void)pHandle;

    return get_time_us() / 1000U;
}

obd_status_t KWP_TMR_DelayMs(void *pHandle, uint32_t delay_ms)
{
    (void)pHandle;

    uint32_t start_us = get_time_us();
    uint32_t delay_us = delay_ms * 1000U;

    /* Blocking wait with microsecond precision - handles wrap-around */
    while ((get_time_us() - start_us) < delay_us) {
        /* Busy wait */
    }

    return OBD_STATUS_OK;
}

obd_status_t KWP_TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    cm_disable_interrupts();

    ctx->timeout_callback = callback;
    ctx->timeout_user_data = pUserData;
    ctx->timeout_start_ms = get_time_us() / 1000U;
    ctx->timeout_duration_ms = timeout_ms;
    ctx->timeout_expired = false;
    ctx->timeout_active = true;

    cm_enable_interrupts();

    return OBD_STATUS_OK;
}

obd_status_t KWP_TMR_StopTimeout(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    ctx->timeout_active = false;

    return OBD_STATUS_OK;
}

bool KWP_TMR_IsTimeoutExpired(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    if (ctx->timeout_active) {
        uint32_t now_ms = get_time_us() / 1000U;
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
