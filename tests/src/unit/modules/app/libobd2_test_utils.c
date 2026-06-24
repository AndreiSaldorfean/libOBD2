/* ================================================ INCLUDES =============================================== */
#include "libobd2_test_utils.h"
#include "l2_iso9141.h"
#include "libobd2_uart_port.h"
#define STM32F4
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/f4/gpio.h"
#include "libopencm3/stm32/common/timer_common_all.h"
#include "libopencm3/stm32/f4/usart.h"

/* ================================================= MACROS ================================================ */
#define TIM2_PRESCALER     (83U)
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
const header_t header_00_ecu =
{
    .fmt = 0xC2,
    .trgAddr = 0x33,
    .srcAddr = 0xF1,
};

const header_t header_00 =
{
    .fmt = 0xC1,
    .trgAddr = 0x33,
    .srcAddr = 0xF1,
};
const obd_data_t data_00 =
{
    .sid = 0x81,
    .param = {0x01}
};
const message_t msg_00_ecu =
{
    .cs = 0X67,
    .data = data_00,
    .header = header_00_ecu
};

const message_t msg_00 =
{
        .cs = 0X67,
        .data = data_00,
        .header = header_00
};

timerCtx_t tmrCtxTx =
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

timerCtx_t tmrCtxRx =
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

obd_timing_ops_t timerOpsTx =
{
        .timer_init         = LIBOBD2_TMR_Init,
        .delay_ms           = LIBOBD2_TMR_DelayMs,
        .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
        .start_timeout      = LIBOBD2_TMR_StartTimeout,
        .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

obd_timing_ops_t timerOpsRx =
{
        .timer_init         = LIBOBD2_TMR_Init,
        .delay_ms           = LIBOBD2_TMR_DelayMs,
        .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
        .start_timeout      = LIBOBD2_TMR_StartTimeout,
        .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

uart_ctx_t uartCtxTx =
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

uart_ctx_t uartCtxRx =
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

obd_uart_ops_t transportOps =
{
    .init        = LIBOBD2_UART_Init,
    .send_byte   = LIBOBD2_UART_WriteByte,
    .recv_byte   = LIBOBD2_UART_RecvByte,
    .send_pulse  = LIBOBD2_UART_SendPulse,
    .switch_mode = LIBOBD2_UART_SwitchMode,
    .flush_rx    = LIBOBD2_UART_FlushRx,
};

l2_iso9141_ctx_t iso9141CtxTx =
{
    .header =
    {
        .fmt     = 0x68,
        .trgAddr = 0x6A,
        .srcAddr = 0xF1,
    },
};

l2_iso9141_ctx_t iso9141CtxRx =
{
    .header =
    {
        .fmt     = 0x48,
        .trgAddr = 0x6B,
        .srcAddr = 0x12,
    },
};

dataLink_if_t dataLink_00 =
{
    .pProtocolCtx     = &iso9141CtxTx,
    .pTimingOps       = &timerOpsTx,
    .pTimingHandle    = &tmrCtxTx,
    .pTransportHandle = &uartCtxTx,
    .pUartOps    = &transportOps,
    .connect          = l2_iso9141_connect,
    .send_request     = l2_iso9141_send_request,
    .recv_response    = l2_iso9141_recv_response,
};

dataLink_if_t dataLink_tx =
{
    .pProtocolCtx     = &iso9141CtxTx,
    .pTimingOps       = &timerOpsTx,
    .pTimingHandle    = &tmrCtxTx,
    .pTransportHandle = &uartCtxTx,
    .pUartOps    = &transportOps,
    .connect          = l2_iso9141_connect,
    .send_request     = l2_iso9141_send_request,
    .recv_response    = l2_iso9141_recv_response,
};
dataLink_if_t dataLink_rx =
{
    .pProtocolCtx     = &iso9141CtxRx,
    .pTimingOps       = &timerOpsRx,
    .pTimingHandle    = &tmrCtxRx,
    .pTransportHandle = &uartCtxRx,
    .pUartOps    = &transportOps,
    .connect          = l2_iso9141_connect,
    .send_request     = l2_iso9141_send_request,
    .recv_response    = l2_iso9141_recv_response,
};
obd_ctx_t ctxRx =
{
        .pDataLink = &dataLink_rx,
        // .pDataLinkHandle = &kwpCtx,
        .connectionStatus = 0
};

obd_ctx_t ctxTx =
{
        .pDataLink = &dataLink_00,
        // .pDataLinkHandle = &kwpCtx,
        .connectionStatus = 0
};

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
