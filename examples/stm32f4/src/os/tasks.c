/* ================================================ INCLUDES =============================================== */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "cdc_device.h"
#include "datalink.h"
#include "ecu_libobd2.h"
#include "iso15031_5.h"
#include "l2_iso9141.h"
#include "libobd2_service_requests.h"
#include "libobd2_services.h"
#include "task.h"
#include "tasks.h"
#include "init.h"
#include "statusRetCodes.h"
#include "libobd2_uart_port.h"
#include "libobd2_timer_port.h"
#include "libobd2.h"
#include "l2_kwp2000.h"
#include "usbd.h"
#include "utils.h"
#include "ecu_l2_iso9141.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/stm32/f4/timer.h"

/* ================================================= MACROS ================================================ */
#define TIM2_PRESCALER      83U

/* ============================================ LOCAL VARIABLES ============================================ */
static volatile bool connectionOk = 0;
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
        .timer_init         = LIBOBD2_TMR_Init,
        .delay_ms           = LIBOBD2_TMR_DelayMs,
        .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
        .start_timeout      = LIBOBD2_TMR_StartTimeout,
        .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

static uart_ctx_t uartCtx =
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

static obd_uart_ops_t transportOps =
{
    .init        = LIBOBD2_UART_Init,
    .send_byte   = LIBOBD2_UART_WriteByte,
    .recv_byte   = LIBOBD2_UART_RecvByte,
    .send_pulse  = LIBOBD2_UART_SendPulse,
    .switch_mode = LIBOBD2_UART_SwitchMode,
};

static l2_iso9141_ctx_t ctx =
{
    .header =
    {
        .fmt     = 0x48,
        .trgAddr = 0x6b,
        .srcAddr = 0x12,
    },
};

static dataLink_if_t dataLink =
{
    .pProtocolCtx     = &ctx,
    .pTimingOps       = &timerOps,
    .pTimingHandle    = &tmrCtx,
    .pTransportHandle = &uartCtx,
    .pUartOps    = &transportOps,
    .connect          = ecu_l2_iso9141_connect,
    .send_request     = ecu_l2_iso9141_send_request,
    .recv_response    = ecu_l2_iso9141_recv_response
};

static obd_ctx_t libobd2Ctx =
{
    .pDataLink = &dataLink,
    .connectionStatus = 0
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void TesterTask(void *param)
{
    (void)param;
    #if 0 /* TODO: Add proper libobd2Ctx for Tester*/
    obd_status_t status;
    uint8_t response[20] = {0};
    size_t respLen = 0;

    (void)param;
    (void)status;

    printf("================== TesterTask ==================\n");

    status = LibOBD2_Init(&libobd2Ctx);
    printf("status= %x\n", status.response);
    printf("timeout= %x\n", status.timeout);

    for (;;)
    {
        status = LibOBD2_RequestService(&libobd2Ctx, OBD_GET_COOLANT_TEMP, response, &respLen);
        printf("status= %x\n", status.response);

        vTaskDelay(1000/portTICK_PERIOD_MS);

        tud_cdc_write_flush();
        tud_task();
    }
    #endif
}

void EcuTask(void *param)
{
    obd_status_t status;

    (void)param;
    (void)status;

    printf("================== TesterTask ==================\n");

    while (1)
    {
        if (!connectionOk)
        {
            status = ECU_LibOBD2_Init(&libobd2Ctx);
            if (status.response == 0 && status.timeout == 0)
            {
                printf("Connection successful!\n");
                printf("status= %x\n", status.response);
                printf("timeout= %x\n", status.timeout);
                connectionOk = 1;
            }
        }
        else
        {
            status = ECU_Listen(&libobd2Ctx);
            if (OBD_ERR_COMM_ECU_CONNECTION_CLSD == status.response)
            {
                printf("Connection closed!\n");
                printf("status= %x\n", status.response);
                printf("timeout= %x\n", status.timeout);
                connectionOk = 0;
            }
            printf("status= %x\n", status.response);
            printf("timeout= %x\n", status.timeout);
        }
    }
}
