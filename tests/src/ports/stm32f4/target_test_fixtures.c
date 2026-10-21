/* ================================================ INCLUDES =============================================== */
#include "target_test_fixtures.h"
#include "datalink.h"
// #include "libobd2_test_utils.h"
#include "init.h"
#define STM32F4
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/cm3/nvic.h"
#include "tusb.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "tasks.h"
/* ================================================= MACROS ================================================ */
#define GREEN_LED GPIO9
#define RED_LED GPIO8

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void gpio_setup(void)
{
    #if 0 /* NOTE: This test blinky had some issues so i commented it temporarily */
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
    #endif
}
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void targetSetUp()
{
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    dataLink_if_t *pDataLinkTx = &dataLink_tx;

    // Added so no test starts untill serial is attached
    LIBOBD_SendByte(pDataLinkRx, 0x42);

    LIBOBD_FlushRx(pDataLinkTx);
    LIBOBD_FlushRx(pDataLinkRx);
}

void targetTearDown()
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

void targetPreTestsInit()
{
    sysInit();

    LIBOBD2_UART_Init(&uartCtxTx);
    LIBOBD2_UART_Init(&uartCtxRx);
    LIBOBD2_TMR_Init(&tmrCtxTx);
    LIBOBD2_TMR_Init(&tmrCtxRx);

    gpio_setup();
}
void targetRunTests()
{
    xTaskCreate(
        TestTask,
        "Test_Task",
        2100,
        NULL,
        tskIDLE_PRIORITY,
        NULL);

    vTaskStartScheduler();

    /* Should never be reached */
    while(true)
    {
        #if defined(LOGGING)
        tud_cdc_write_flush();
        tud_task();
        #endif /* LOGGING */
    }
}
