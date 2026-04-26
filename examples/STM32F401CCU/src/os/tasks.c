/* ================================================ INCLUDES =============================================== */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "cdc_device.h"
#include "datalink.h"
#include "ecu_libobd2.h"
#include "iso15031_5.h"
#include "l2_iso9141.h"
#include "task.h"
#include "tasks.h"
#include "init.h"
#include "statusRetCodes.h"
#include "uart_kwp_transport_port.h"
#include "libopencm3/stm32/f4/timer.h"
#include "kwp_timer.h"
#include "libobd2.h"
#include "l2_kwp2000.h"
#include "usbd.h"
#include "utils.h"

/* ================================================= MACROS ================================================ */
#define GPIOC_MODE_REGISTER *(volatile uint32_t*)(uintptr_t)(0x40020800UL)
#define GPIOC_MODER13_SHIFT (26)
#define GPIOC_MODER13_MASK  (0xC000000)
#define GPIOC_OTYPE *(volatile uint32_t*)(uintptr_t)(0x40020800UL + 0x4)
#define TIM2_PRESCALER      83U
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void configDataLinkTester(dataLink_if_t* dl)
{
    static timerCtx_t tmrCtx =
    {
        .timeout_active    = false,
        .timeout_expired   = false,
        .timeout_callback  = NULL,
        .timeout_user_data = NULL,
        .timeout_start_ms  = 0,
        .timerClk          = RCC_TIM2,
        .rstTimer          = RST_TIM2,
        .timer             = TIM2,
        .timerPrescaler    = TIM2_PRESCALER,
        .event             = TIM_EGR_UG,
        .flag              = TIM_SR_UIF
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
        .switch_mode = UART_KWP_SwitchMode,
    };


    static l2_iso9141_ctx_t ctx =
    {
        .header =
        {
            .fmt     = 0x68,
            .trgAddr = 0x6A,
            .srcAddr = 0x12,
            .len     = 1
        },
    };

    dl->pProtocolCtx     = &ctx;
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
void TesterTask(void *param)
{
    obd_status_t status;
    dataLink_if_t dataLink;

    (void)param;
    (void)status;

    printf("================== TesterTask ==================\n");

    configDataLinkTester(&dataLink);

    obd_ctx_t ctx =
    {
        .pDataLink = &dataLink,
        .connectionStatus = 0
    };

    status = LibOBD2_Init(&ctx);
    printf("status= %x\n", status.response);
    printf("timeout= %x\n", status.timeout);

    // obd_request_t request =
    // {
    //     .sid = SID_SHOW_CURRENT_DATA,
    //     .param = {PID_01_COOLANT_TEMP},
    // };
    // obd_response_t response = {0};
    // size_t respLen = 0;
    //
    // for (;;)
    // {
    //     status = LibOBD2_RequestService(&ctx, &request, 1, &response, &respLen);
    //     printf("status= %x\n", status.response);
    //
    //     vTaskDelay(1000/portTICK_PERIOD_MS);
    //
    //     tud_cdc_write_flush();
    //     tud_task();
    // }
}

void EcuTask(void *param)
{
    obd_status_t status;
    dataLink_if_t dataLink;

    (void)param;
    (void)status;

    printf("================== TesterTask ==================\n");

    configDataLinkTester(&dataLink);

    obd_ctx_t ctx =
    {
        .pDataLink = &dataLink,
        .connectionStatus = 0
    };

    status = ECU_LibOBD2_Init(&ctx);
    printf("status= %x\n", status.response);
    printf("timeout= %x\n", status.timeout);

    // obd_request_t request =
    // {
    //     .sid = SID_SHOW_CURRENT_DATA,
    //     .param = {PID_01_COOLANT_TEMP},
    // };
    // obd_response_t response = {0};
    // size_t respLen = 0;

    // for (;;)
    // {
    //     status = LibOBD2_RequestService(&ctx, &request, 1, &response, &respLen);
    //     printf("status= %x\n", status.response);
    //
    //     vTaskDelay(1000/portTICK_PERIOD_MS);
    //
    //     tud_cdc_write_flush();
    //     tud_task();
    // }
}
