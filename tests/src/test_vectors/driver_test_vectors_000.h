#ifndef DRIVER_TEST_VECTORS_000_H
#define DRIVER_TEST_VECTORS_000_H

/* ================================================ INCLUDES =============================================== */
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_if.h"
#define STM32F4
#include "libopencm3/stm32/f4/memorymap.h"
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/common/timer_common_all.h"
#include "libopencm3/stm32/f4/gpio.h"
#include "libopencm3/stm32/f4/usart.h"
#include <string.h>

/* ================================================= MACROS ================================================ */
#define TIM2_PRESCALER (83U)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
static timerCtx_t tmr_ctx_tester =
{
    .timeout_active      = false,
    .timeout_expired     = false,
    .timeout_duration_ms = 0,
    .timeout_callback    = NULL,
    .timeout_user_data   = NULL,
    .timeout_start_ms    = 0,
    .timerClk            = RCC_TIM2,
    .rstTimer            = RST_TIM2,
    .timer               = TIM2,
    .timerPrescaler      = TIM2_PRESCALER,
    .event               = TIM_EGR_UG,
    .flag                = TIM_SR_UIF
};

static timerCtx_t tmr_ctx_ecu =
{
    .timeout_active      = false,
    .timeout_expired     = false,
    .timeout_duration_ms = 0,
    .timeout_callback    = NULL,
    .timeout_user_data   = NULL,
    .timeout_start_ms    = 0,
    /* TIM1 is 16-bit (wraps every ~65 ms) and cannot track P2/P3 timeouts.
     * Share TIM2's 32-bit free-running counter; LIBOBD2_TMR_Init skips HW
     * re-init when TIM2 is already running so the counter is not reset. */
    .timerClk            = RCC_TIM2,
    .rstTimer            = RST_TIM2,
    .timer               = TIM2,
    .timerPrescaler      = TIM2_PRESCALER,
    .event               = TIM_EGR_UG,
    .flag                = TIM_SR_UIF
};

static uart_ctx_t uart_ctx_tester =
{
        .usartClk    = RCC_USART1,
        .usartNum    = USART1,
        .baudRate    = 10400,
        .dataBits    = 8,
        .stopBits    = USART_STOPBITS_1,
        .mode        = USART_MODE_TX_RX,
        .parity      = USART_PARITY_NONE,
        .flowControl = USART_FLOWCONTROL_NONE,
        .usartTxPin  = GPIO9,
        .usartRxPin  = GPIO10,

        .gpioRcc      = RCC_GPIOA,
        .gpioOutType  = GPIO_OTYPE_OD,
        .gpioOutSpeed = GPIO_OSPEED_2MHZ,
        .gpio         = GPIOA,
};

static uart_ctx_t uart_ctx_ecu =
{
        .usartClk    = RCC_USART2,
        .usartNum    = USART2,
        .baudRate    = 10400,
        .dataBits    = 8,
        .stopBits    = USART_STOPBITS_1,
        .mode        = USART_MODE_TX_RX,
        .parity      = USART_PARITY_NONE,
        .flowControl = USART_FLOWCONTROL_NONE,
        .usartTxPin  = GPIO2,
        .usartRxPin  = GPIO3,

        .gpioRcc      = RCC_GPIOA,
        .gpioOutType  = GPIO_OTYPE_OD,
        .gpioOutSpeed = GPIO_OSPEED_2MHZ,
        .gpio         = GPIOA,
};

/****************************** OPS & CONFIGS ******************************/
static obd_timing_ops_t tmr_ops =
{
        .timer_init         = LIBOBD2_TMR_Init,
        .delay_ms           = LIBOBD2_TMR_DelayMs,
        .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
        .start_timeout      = LIBOBD2_TMR_StartTimeout,
        .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

static obd_uart_ops_t uart_ops =
{
    .init        = LIBOBD2_UART_Init,
    .send_byte   = LIBOBD2_UART_WriteByte,
    .recv_byte   = LIBOBD2_UART_RecvByte,
    .send_pulse  = LIBOBD2_UART_SendPulse,
    .switch_mode = LIBOBD2_UART_SwitchMode,
    .flush_rx    = LIBOBD2_UART_FlushRx,
};

#endif /* TEST_VECTORS_000_H */

