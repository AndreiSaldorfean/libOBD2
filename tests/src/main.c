/* ================================================ INCLUDES =============================================== */
#include "init.h"
#include "libobd2_test_utils.h"
#include "timer_test.h"
#include "uart_kwp_transport_port.h"
#include "unity.h"
#include "unity_internals.h"
#include "test_libobd2.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "tasks.h"
#define STM32F4
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/cm3/nvic.h"
#include "tusb.h"
#include "l2_kwp_test.h"
#include "l2_kwp_utils_test.h"
#include "task.h"
#include "tasks.h"


/* ================================================= MACROS ================================================ */
#define GREEN_LED GPIO9
#define RED_LED GPIO8

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void gpio_setup(void)
{
    dataLink_if_t *pDataLinkTx = &dataLink_tx;

    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GREEN_LED | RED_LED);
    gpio_clear(GPIOB, GREEN_LED | RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_set(GPIOB, GREEN_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_clear(GPIOB, GREEN_LED | RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_set(GPIOB, RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_clear(GPIOB, GREEN_LED | RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_set(GPIOB, GREEN_LED| RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_clear(GPIOB, GREEN_LED | RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_set(GPIOB, GREEN_LED| RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_clear(GPIOB, GREEN_LED | RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);

    gpio_set(GPIOB, GREEN_LED| RED_LED);
    LIBOBD_Delay(pDataLinkTx, 100);
    gpio_clear(GPIOB, GREEN_LED | RED_LED);
}
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
void setUp(void)
{
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    dataLink_if_t *pDataLinkTx = &dataLink_tx;

    // Added so no test starts untill serial is attached
    LIBOBD_SendByte(pDataLinkRx, 0x42);

    LIBOBD_FlushRx(pDataLinkTx);
    LIBOBD_FlushRx(pDataLinkRx);
}

void tearDown(void)
{
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    dataLink_if_t *pDataLinkTx = &dataLink_tx;

    LIBOBD_FlushRx(pDataLinkTx);
    LIBOBD_FlushRx(pDataLinkRx);
    if (Unity.CurrentTestFailed)
    {
        gpio_set(GPIOB, RED_LED);
        LIBOBD_Delay(pDataLinkTx, 2000);
        gpio_clear(GPIOB, RED_LED | GREEN_LED);
    }
    else
    {
        gpio_set(GPIOB, GREEN_LED);
        LIBOBD_Delay(pDataLinkTx, 2000);
        gpio_clear(GPIOB, RED_LED | GREEN_LED);
    }
}

/* ================================================ MODULE API ============================================= */
int main()
{
    TaskHandle_t libobd2TestTaskHandle = NULL;
    // TaskHandle_t l2KwpTestTaskHandle   = NULL;
    // TaskHandle_t uartTestTaskHandle    = NULL;
    uint32_t status = 0;

    sysInit();

    UART_KWP_Init(&uartCtxTx);
    UART_KWP_Init(&uartCtxRx);
    KWP_TMR_Init(&tmrCtxTx);
    KWP_TMR_Init(&tmrCtxRx);

    gpio_setup();

    /* Create LIBOBD2 suite first but at lower priority — it will only run
     * once L2_KWP_TestTask finishes and deletes itself. */
    status = xTaskCreate(
        LIBOBD2_TestTask,
        "Libobd2_Test_Task",
        2100,
        NULL,
        tskIDLE_PRIORITY + 1,           /* lower: waits until L2_KWP_TestTask is gone */
        &libobd2TestTaskHandle);

    if (status != pdPASS)
    {
        while (1)
            ;
    }

    /* L2_KWP suite runs first because it has higher priority.
     * Sub-tasks it spawns are at tskIDLE_PRIORITY+3, so they still preempt
     * this task normally. LIBOBD2_TestTask never gets scheduled until this
     * task calls vTaskDelete(NULL). */
    // status = xTaskCreate(
    //     L2_KWP_TestTask,
    //     "L2_kwp_Test_Task",
    //     1024,
    //     NULL,
    //     tskIDLE_PRIORITY + 1,       /* higher: runs before LIBOBD2_TestTask */
    //     &l2KwpTestTaskHandle);
    //
    // if (status != pdPASS)
    // {
    //     while (1)
    //         ;
    // }

    // status = xTaskCreate(
    //     UART_TestTask,
    //     "UART_TestTask",
    //     512,
    //     NULL,
    //     tskIDLE_PRIORITY + 1,       /* higher: runs before LIBOBD2_TestTask */
    //     &uartTestTaskHandle);
    //
    // if (status != pdPASS)
    // {
    //     while (1)
    //         ;
    // }

    vTaskStartScheduler();


    /* Should never be reached */
    while(true)
    {
        #if !defined(DEBUG)
        tud_cdc_write_flush();
        tud_task();
        #endif /* DEBUG */
    }

    return 0;
}
