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
#define GPIOC_MODE_REGISTER *(volatile uint32_t*)(uintptr_t)(0x40020800UL)
#define GPIOC_MODER13_SHIFT (26)
#define GPIOC_MODER13_MASK  (0xC000000)
#define GPIOC_OTYPE *(volatile uint32_t*)(uintptr_t)(0x40020800UL + 0x4)
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void configDummyTranciever(timerCtx_t* tmrCtx, uartKwp_ctx_t *uartCtx)
{
        tmrCtx->timeout_active    = false;
        tmrCtx->timeout_expired   = false;
        tmrCtx->timeout_callback  = NULL;
        tmrCtx->timeout_user_data = NULL;
        tmrCtx->timeout_start_ms = 0;

        uartCtx->usartClk    = RCC_USART1;
        uartCtx->usartNum    = USART1;
        uartCtx->baudRate    = 10400;
        uartCtx->dataBits    = 8;
        uartCtx->stopBits    = USART_STOPBITS_1;
        uartCtx->mode        = USART_MODE_TX_RX;
        uartCtx->parity      = USART_PARITY_NONE;
        uartCtx->flowControl = USART_FLOWCONTROL_NONE;
        uartCtx->usartTxPin  = GPIO9;
        uartCtx->usartRxPin  = GPIO10;

        uartCtx->gpioRcc     = RCC_GPIOA;
        uartCtx->gpio        = GPIOA;
}

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

void DummyTransmitter(void *param)
{
    obd_status_t status;
    uint8_t buffer[10];

    (void)param;
    (void)status;

    timerCtx_t tmrCtx;
    uartKwp_ctx_t uartCtx;

    configDummyTranciever(&tmrCtx, &uartCtx);

    {
        rcc_periph_clock_enable(RCC_GPIOC);

        uint32_t pinmode = (GPIOC_MODER13_MASK & (0x01<<GPIOC_MODER13_SHIFT));

        GPIOC_MODE_REGISTER |= pinmode;
    }

    KWP_TMR_Init(&tmrCtx);
    UART_KWP_Init(&uartCtx);

    KWP_TMR_DelayMs(&tmrCtx, 25);
    GPIOC_ODR ^= (1 << 13);

    for (int i=0; i < 5; i++)
    {
        /* Receive byte (blocking) */
        UART_KWP_RecvByte(&uartCtx, buffer + i);
    }


    GPIOC_ODR ^= (1 << 13);
    KWP_TMR_DelayMs(&tmrCtx, 25);

    UART_KWP_WriteByte(&uartCtx, 0x83);
    UART_KWP_WriteByte(&uartCtx, 0xF1);
    UART_KWP_WriteByte(&uartCtx, 0x10);
    UART_KWP_WriteByte(&uartCtx, 0xC1);
    UART_KWP_WriteByte(&uartCtx, 0xE9);
    UART_KWP_WriteByte(&uartCtx, 0x8F);
    UART_KWP_WriteByte(&uartCtx, 0xBD);

    GPIOC_ODR ^= (1 << 13);
}

void DummyReceiver(void *param)
{
    (void)param;

    timerCtx_t tmrCtx;
    uartKwp_ctx_t uartCtx;

    configDummyTranciever(&tmrCtx, &uartCtx);

    KWP_TMR_Init(&tmrCtx);
    UART_KWP_Init(&uartCtx);

    uint8_t rxBuffer[16] = {0};
    (void)rxBuffer;
    uint8_t idx = 0;

    for (;;)
    {
        uint8_t byte;

        /* Receive byte (blocking) */
        UART_KWP_RecvByte(&uartCtx, &byte);

        /* Store in buffer */
        rxBuffer[idx % 16] = byte;
        idx++;

        /* Optional: Print received byte via some debug output */
        /* For now, just toggle LED or set breakpoint here to inspect rxBuffer */

        /* If we received 6 bytes (expected message length), reset */
        if (idx >= 6)
        {
            /* Check if received: 0x83 0xF1 0x10 0xC1 0x8F 0xBD */
            /* Set breakpoint here to inspect rxBuffer */
            idx = 0;
        }
    }
}
