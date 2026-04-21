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
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
void setUp(void) { }

void tearDown(void) { }

/* ================================================ MODULE API ============================================= */
int main()
{
    sysInit();

    UART_KWP_Init(&uartCtxTx);
    UART_KWP_Init(&uartCtxRx);
    KWP_TMR_Init(&tmrCtxTx);
    KWP_TMR_Init(&tmrCtxRx);

    TaskHandle_t l2KwpTestTaskHandle   = NULL;
    TaskHandle_t libobd2TestTaskHandle = NULL;
    uint32_t status = 0;

    /* Create LIBOBD2 suite first but at lower priority — it will only run
     * once L2_KWP_TestTask finishes and deletes itself. */
    status = xTaskCreate(
        LIBOBD2_TestTask,
        "Libobd2_Test_Task",
        1024,
        NULL,
        tskIDLE_PRIORITY,           /* lower: waits until L2_KWP_TestTask is gone */
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
    status = xTaskCreate(
        L2_KWP_TestTask,
        "L2_kwp_Test_Task",
        1024,
        NULL,
        tskIDLE_PRIORITY + 1,       /* higher: runs before LIBOBD2_TestTask */
        &l2KwpTestTaskHandle);

    if (status != pdPASS)
    {
        while (1)
            ;
    }

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
