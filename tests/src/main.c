/* ================================================ INCLUDES =============================================== */
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
#include "syscalls.h"
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

    TaskHandle_t testerTaskHandle = NULL;
    uint32_t status = 0;

    status = xTaskCreate(
        TestTask,
        "Receiver Task",
        1024,
        NULL,
        tskIDLE_PRIORITY,
        &testerTaskHandle);

    if (status)
    {
        syscalls_init();
        vTaskStartScheduler();
    }


    while(true)
    {
        #if !defined(DEBUG)
        tud_cdc_write_flush();
        tud_task();
        #endif /* DEBUG */
    }

    return 0;
}
