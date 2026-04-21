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

#include <stdint.h>
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

#if !defined(DEBUG)
static void usart_setup(void)
{
    /* Use internal HSI oscillator - works on all F401CCU boards without crystal */
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    /* Enable GPIO clocks for USB */
    rcc_periph_clock_enable(RCC_GPIOA);

    /*
     * Force USB re-enumeration by pulling D+ (PA12) LOW briefly.
     * This signals disconnect to the host, forcing it to re-enumerate
     * when we release it. Needed after MCU reset via debugger.
     */
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO12);
    gpio_clear(GPIOA, GPIO12);
    for (volatile int i = 0; i < 800000; i++) { __asm__("nop"); }  /* ~50ms delay */

    /* Setup USB pins PA11 (D-) and PA12 (D+) BEFORE enabling USB clock */
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
    gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12);

    /* Enable USB OTG FS clock */
    rcc_periph_clock_enable(RCC_OTGFS);

    /* Initialize TinyUSB */
    tusb_init();

    /* Enable USB interrupt after initialization */
    nvic_enable_irq(NVIC_OTG_FS_IRQ);
}
#endif /* DEBUG */
/* ================================================ MODULE API ============================================= */

int main()
{
    #if !defined(DEBUG)
	usart_setup();

	/* Disable stdout buffering for immediate printf output */
	setbuf(stdout, NULL);
    #endif /* DEBUG */

    UART_KWP_Init(&uartCtxTx);
    UART_KWP_Init(&uartCtxRx);
    KWP_TMR_Init(&tmrCtx);

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
