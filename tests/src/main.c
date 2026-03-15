/* ================================================ INCLUDES =============================================== */
#include "test_timer.h"
#include "unity.h"
#include "unity_internals.h"
#include "test_libobd2.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include <stdint.h>
#define STM32F4
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/cm3/nvic.h"
#include "tusb.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
void setUp(void) { }

void tearDown(void) { }

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
/* ================================================ MODULE API ============================================= */

int main()
{
	usart_setup();

	/* Disable stdout buffering for immediate printf output */
	setbuf(stdout, NULL);

    printf("============= UNIT BEGIN ==============\n");

    UNITY_BEGIN();
    //
    // RUN_TEST(test_LIBOBD2_0);
    // RUN_TEST(test_LIBOBD2_1);
    // RUN_TEST(test_TIMER_0);
    // RUN_TEST(test_TIMER_1);
    RUN_TEST(test_Tester_ECU);

    int result = UNITY_END();

    printf("============== UNIT END ===============\n");

    while(true)
    {

        tud_cdc_write_flush();
        tud_task();
    }

    return result;
}
