/* ================================================ INCLUDES =============================================== */
#include "libobd2_test_utils.h"
#define STM32F4
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/f4/gpio.h"
#include "libopencm3/stm32/common/timer_common_all.h"
#include "libopencm3/stm32/f4/usart.h"

/* ================================================= MACROS ================================================ */
#define TIM2_PRESCALER     (83U)
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
const header_t header_00 =
{
    .fmt = {.val = 0xC1},
    .trgAddr = 0x33,
    .srcAddr = 0xF1,
};
const obd_request_t request_00 =
{
    .sid = 0x81,
    .param = {0x01}
};
const data_t data_00 =
{
    .req = request_00,
    .len = 0x1,
};
const message_t msg_00 =
{
        .cs = 0X67,
        .data = data_00,
        .header = header_00
};

timerCtx_t tmrCtx =
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
obd_timing_ops_t timerOps =
{
        .timer_init         = KWP_TMR_Init,
        .delay_ms           = KWP_TMR_DelayMs,
        .get_time_ms        = KWP_TMR_GetTimeMs,
        .is_timeout_expired = KWP_TMR_IsTimeoutExpired,
        .start_timeout      = KWP_TMR_StartTimeout,
        .stop_timeout       = KWP_TMR_StopTimeout,
};
uartKwp_ctx_t uartCtxTx =
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

        .gpioRcc     = RCC_GPIOA,
        .gpio        = GPIOA,
};
// static timerCtx_t tmrCtxTx =
// {
//     .timeout_active    = false,
//     .timeout_expired   = false,
//     .timeout_callback  = NULL,
//     .timeout_user_data = NULL,
//     .timeout_start_ms  = 0,
//     .timerClk          = RCC_TIM2,
//     .rstTimer          = RST_TIM2,
//     .timer             = TIM2,
//     .timerPrescaler    = TIM2_PRESCALER,
//     .event             = TIM_EGR_UG,
//     .flag              = TIM_SR_UIF
// };
uartKwp_ctx_t uartCtxRx =
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

        .gpioRcc     = RCC_GPIOA,
        .gpio        = GPIOA,
};
// static timerCtx_t tmrCtxRx =
// {
//     .timeout_active    = false,
//     .timeout_expired   = false,
//     .timeout_callback  = NULL,
//     .timeout_user_data = NULL,
//     .timeout_start_ms  = 0,
//     .timerClk          = RCC_TIM3,
//     .rstTimer          = RST_TIM3,
//     .timer             = TIM3,
//     .timerPrescaler    = TIM2_PRESCALER,
//     .event             = TIM_EGR_UG,
//     .flag              = TIM_SR_UIF
// };
obd_transport_ops_t transportOps =
{
    .init        = UART_KWP_Init,
    .send_byte   = UART_KWP_WriteByte,
    .recv_byte   = UART_KWP_RecvByte,
    .send_pulse  = UART_KWP_SendPulse,
    .switch_mode = UART_KWP_SwitchMode,
};
l2_kwp_ctx_t kwpCtx =
{
    .conStatus = {0U},
    .header =
    {
        .fmt = {0x10U},
        .trgAddr = 0x33,
        .srcAddr = 0xF1,
        .len = 1
    },
};
dataLink_if_t dataLink_00 =
{
    .pProtocolCtx     = &kwpCtx,
    .pTimingOps       = &timerOps,
    .pTimingHandle    = &tmrCtx,
    .pTransportHandle = &uartCtxTx,
    .pTransportOps    = &transportOps,
    .connect          = l2_kwp_connect,
    .send_request     = l2_kwp_send_request,
    .recv_response    = l2_kwp_recv_response,
};
dataLink_if_t dataLink_tx =
{
    .pProtocolCtx     = &kwpCtx,
    .pTimingOps       = &timerOps,
    .pTimingHandle    = &tmrCtx,
    .pTransportHandle = &uartCtxTx,
    .pTransportOps    = &transportOps,
    .connect          = l2_kwp_connect,
    .send_request     = l2_kwp_send_request,
    .recv_response    = l2_kwp_recv_response,
};
dataLink_if_t dataLink_rx =
{
    .pProtocolCtx     = &kwpCtx,
    .pTimingOps       = &timerOps,
    .pTimingHandle    = &tmrCtx,
    .pTransportHandle = &uartCtxRx,
    .pTransportOps    = &transportOps,
    .connect          = l2_kwp_connect,
    .send_request     = l2_kwp_send_request,
    .recv_response    = l2_kwp_recv_response,
};
#if 0
static obd_ctx_t ctx =
{
        .pDataLink = &dataLink_00,
        // .pDataLinkHandle = &kwpCtx,
        .connectionStatus = 0
};
#endif

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
