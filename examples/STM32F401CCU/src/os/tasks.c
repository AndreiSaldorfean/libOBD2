/* ================================================ INCLUDES =============================================== */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "data_link_if.h"
#include "iso15031_5.h"
#include "task.h"
#include "tasks.h"
#include "init.h"
#include "srv_status.h"
#include "uart_kwp_transport_port.h"
#include "kwp_timer.h"
#include "libobd2.h"
#include "l2_kwp.h"
#include "utils.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void configDataLink(dataLink_if_t* dl)
{
    static timerCtx_t tmrCtx =
    {
        .timeout_active    = false,
        .timeout_expired   = false,
        .timeout_callback  = NULL,
        .timeout_user_data = NULL,
        .timeout_start_ms = 0,
    };

    static obd_timing_ops_t timerOps =
    {
            .timer_init         = KWP_TMR_Init,
            .delay_ms           = KWP_TMR_DelayMs,
            .get_time_ms        = KWP_TMR_GetTimeMs,
            .is_timeout_expired = KWP_TMR_IsTimeoutExpired,
            .start_timeout      = KWP_TMR_StartTimeout,
            .stop_timeout       = KWP_TMR_StopTimeout,
    };

    static uartKwp_ctx_t uartCtx =
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

    static obd_transport_ops_t transportOps =
    {
        .init        = UART_KWP_Init,
        .send_byte   = UART_KWP_WriteByte,
        .recv_byte   = UART_KWP_RecvByte,
        .send_pulse  = UART_KWP_SendPulse,
        .change_baud = UART_KWP_ChangeBaud,
    };


    static l2_kwp_ctx_t kwpCtx =
    {
        .conStatus = {0u},
        .header    =
        {
            .fmt     = {0x10U},
            .trgAddr = 0x33,
            .srcAddr = 0xF1,
            .len     = 1
        },
    };


    dl->pProtocolCtx     = &kwpCtx;
    dl->pTimingOps       = &timerOps;
    dl->pTimingHandle    = &tmrCtx;
    dl->pTransportHandle = &uartCtx;
    dl->pTransportOps    = &transportOps;
    dl->connect          = l2_kwp_connect;
    dl->send_request     = l2_kwp_send_request;
    dl->recv_response    = l2_kwp_recv_response;
}
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void TransceiverTask(void *param)
{
    obd_status_t status;
    dataLink_if_t dataLink;

    (void)param;
    (void)status;

    configDataLink(&dataLink);

    obd_ctx_t ctx =
    {
            .pDataLink = &dataLink,
            .connectionStatus = 0
    };

    status = LibOBD2_Init(&ctx);

    obd_request_t request =
    {
            .sid = SID_SHOW_CURRENT_DATA,
            .param = {PID_01_COOLANT_TEMP},
    };
    obd_response_t response = {0};
    size_t respLen = 0;

    for (;;)
    {
        LibOBD2_RequestService(&ctx, &request, 1, &response, &respLen);

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}
