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
#include "l2_kwp_test.h"

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

    /* TODO: Add proper unit tests and turn examples module to proper examples not bad integration tests */
    #if 0
    RUN_TEST(test_LIBOBD2_0);
    RUN_TEST(test_LIBOBD2_1);
    RUN_TEST(test_TIMER_0);
    RUN_TEST(test_TIMER_1);
    #endif

    /* ======================= Unit tests ======================= */
    RUN_TEST(test_Tester_ECU);
    RUN_TEST(test_L2_KWP_ComputeChecksum_000);
    RUN_TEST(test_L2_KWP_SendMessage_000);
    RUN_TEST(test_L2_KWP_RecvMessage_000);
#if defined(SPT_FAST_INIT)
    RUN_TEST(test_L2_KWP_SRV_StartCommunication_000);
    RUN_TEST(test_L2_KWP_SRV_SendData_000);
    RUN_TEST(test_L2_KWP_FastInit_000);
    RUN_TEST(test_L2_KWP_IdleBasedOnConnStatus_000);
#endif /* SPT_FAST_INIT */
#if defined(SPT_CHANGE_TIMING_PARAM)
    void test_L2_KWP_SRV_AccessTimingParameter_000);
#endif /* SPT_CHANGE_TIMING_PARAM */
#if defined(SPT_5BAUD_INIT)
    void test_L2_KWP_5BaudInit_000);
#endif /* SPT_5BAUD_INIT */
    RUN_TEST(test_L2_KWP_Init_000);
    RUN_TEST(test_L2_KWP_ReadHeader_000);
    RUN_TEST(test_PrepareMessage_000);
    /* ======================= Unit tests ======================= */

    RUN_TEST(test_Tester_ECU);

    int result = UNITY_END();

    while(true)
    {
        tud_cdc_write_flush();
        tud_task();
    }

    return result;
}
