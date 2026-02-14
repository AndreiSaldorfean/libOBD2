/* ================================================ INCLUDES =============================================== */
#include "kwp_timer.h"
#include <stddef.h>
#define STM32F4
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/stm32/f4/nvic.h"
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/f4/timer.h"
#include "srv_status.h"

/* ================================================= MACROS ================================================ */
#define TIMER_FREQ_HZ       1000U   /* 1kHz = 1ms per tick */
#define TICKS_PER_MS        1U

/* ============================================ LOCAL VARIABLES ============================================ */
static volatile uint32_t g_ms_counter = 0;
static timerCtx_t *g_timer_ctx = NULL;  /* For ISR access */

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void tim_setup(void);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void tim_setup(void)
{
	/* Enable TIM2 clock. */
	rcc_periph_clock_enable(RCC_TIM2);

	/* Enable TIM2 interrupt. */
	nvic_enable_irq(NVIC_TIM2_IRQ);

	/* Reset TIM2 peripheral to defaults. */
	rcc_periph_reset_pulse(RST_TIM2);

	/*
	 * Configure timer for 1ms tick rate.
	 * APB1 timer clock = APB1_freq * 2 (when APB1 prescaler != 1)
	 * Prescaler = (timer_clk / desired_freq) - 1
	 * For 1kHz (1ms period): prescaler = (timer_clk / 1000) - 1
	 */
	timer_set_prescaler(TIM2, ((rcc_apb1_frequency * 2) / TIMER_FREQ_HZ) - 1);

	/* Disable preload. */
	timer_disable_preload(TIM2);
	timer_continuous_mode(TIM2);

	/* Set period to 1 tick - overflow every 1ms */
	timer_set_period(TIM2, TICKS_PER_MS);

	/* Counter enable. */
	timer_enable_counter(TIM2);

	/* Enable update interrupt (fires on overflow) */
	timer_enable_irq(TIM2, TIM_DIER_UIE);
}

/**
 * @brief TIM2 interrupt handler - called every 1ms
 */
void tim2_isr(void)
{
	if (timer_get_flag(TIM2, TIM_SR_UIF)) {
		timer_clear_flag(TIM2, TIM_SR_UIF);

		/* Increment millisecond counter */
		g_ms_counter++;

		/* Check timeout */
		if (g_timer_ctx != NULL && g_timer_ctx->timeout_active) {
			if (g_ms_counter >= g_timer_ctx->timeout_target_ms) {
				g_timer_ctx->timeout_active = false;
				g_timer_ctx->timeout_expired = true;

				/* Call callback if registered */
				if (g_timer_ctx->timeout_callback != NULL) {
					g_timer_ctx->timeout_callback(g_timer_ctx->timeout_user_data);
				}
			}
		}
	}
}

/* ================================================ MODULE API ============================================= */
obd_status_t KWP_TMR_Init(void *pHandle)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    /* Store context for ISR access */
    g_timer_ctx = ctx;
    g_ms_counter = 0;

    /* Setup the hardware timer */
    tim_setup();

    return OBD_STATUS_OK;
}

uint32_t KWP_TMR_GetTimeMs(void *pHandle)
{
    (void)pHandle;  /* Not needed, using global counter */

    return g_ms_counter;
}

obd_status_t KWP_TMR_DelayMs(void *pHandle, uint32_t delay_ms)
{
    (void)pHandle;

    uint32_t start_ms = g_ms_counter;

    /* Blocking wait - handles wrap-around correctly */
    while ((g_ms_counter - start_ms) < delay_ms) {
        /* Could add __WFI() here to save power */
    }

    return OBD_STATUS_OK;
}

obd_status_t KWP_TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData)
{
    timerCtx_t *ctx = (timerCtx_t*)pHandle;

    /* Configure timeout */
    ctx->timeout_callback = callback;
    ctx->timeout_user_data = pUserData;
    ctx->timeout_target_ms = g_ms_counter + timeout_ms;
    ctx->timeout_expired = false;
    ctx->timeout_active = true;

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

    return ctx->timeout_expired;
}
