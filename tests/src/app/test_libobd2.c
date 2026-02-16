/* ================================================ INCLUDES =============================================== */
#include "test_libobd2.h"
#include "data_link_if.h"
#include "kwp_timer.h"
#include "libobd2.h"
#include "timing_if.h"
#define STM32F4
#include "libopencm3/stm32/f4/memorymap.h"
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/f4/usart.h"
#include "libopencm3/stm32/f4/gpio.h"
#include "srv_status.h"
#include "transport_if.h"
#include "iso15031_5.h"
#include "tusb.h"
#include "unity.h"
#include <stdio.h>
#include "uart_kwp_transport_port.h"
#include "l2_kwp.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_LIBOBD2_0(void)
{
    obd_status_t status;

    timerCtx_t tmrCtx =
    {
        .timeout_active = false,
        .timeout_expired = false,
        .timeout_target_ms = 0,
    };

    obd_timing_ops_t timerOps =
    {
            .timer_init = KWP_TMR_Init,
            .delay_ms = KWP_TMR_DelayMs,
            .get_time_ms = KWP_TMR_GetTimeMs,
            .is_timeout_expired = KWP_TMR_IsTimeoutExpired,
            .start_timeout = KWP_TMR_StartTimeout,
            .stop_timeout = KWP_TMR_StopTimeout,
    };

    uartKwp_ctx_t uartCtx =
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

    obd_transport_ops_t transportOps =
    {
        .init      = UART_KWP_Init,
        .send_byte = UART_KWP_WriteByte,
        .recv_byte = UART_KWP_RecvByte,
        .send_pulse = UART_KWP_SendPulse,
        .switch_mode = UART_KWP_ChangeBaud,
    };


    l2_kwp_ctx_t kwpCtx =
    {
        .conStatus = {0u},
        .header =
        {
            .fmt = {0x10U},
            .trgAddr = 0x33,
            .srcAddr = 0xF1,
            .len = 1
        },
    };


    dataLink_if_t dataLink =
    {
        .pProtocolCtx = &kwpCtx,
        .pTimingOps = &timerOps,
        .pTimingHandle = &tmrCtx,
        .pTransportHandle = &uartCtx,
        .pTransportOps = &transportOps,
        .connect = l2_kwp_connect,
        .send_request = l2_kwp_send_request,
        .recv_response = l2_kwp_recv_response,
    };

    obd_ctx_t ctx =
    {
            .pDataLink = &dataLink,
            // .pDataLinkHandle = &kwpCtx,
            .connectionStatus = 0
    };

    status = LibOBD2_Init(&ctx);
    TEST_ASSERT_EQUAL_UINT32(OBD_ERR_NULL_PTR, status);
}

void test_LIBOBD2_1(void)
{
    uint32_t respBuffer[256];

    obd_transport_ops_t transportOps =
    {
        .init      = UART_KWP_Init,
        .send_byte = UART_KWP_WriteByte,
        .recv_byte = UART_KWP_RecvByte
    };
    dataLink_if_t dataLink =
    {
        .pTransportHandle = NULL,
        .pTransportOps = &transportOps,
        .connect = NULL,
        .send_request = NULL,
        .recv_response = NULL,
    };
    obd_ctx_t ctx =
    {
        .pDataLink = &dataLink,
        // .pDataLinkHandle = NULL,
        .connectionStatus = 0
    };
    size_t len;
    obd_status_t status;

    obd_request_t request = {
        .sid = SID_SHOW_CURRENT_DATA,
        .param = {PID_01_COOLANT_TEMP}
    };

    obd_response_t response = {0};

    status = LibOBD2_RequestService(&ctx, &request, 2, &response, &len);
    TEST_ASSERT_EQUAL_UINT32(OBD_ERR_NULL_PTR, status);
}
