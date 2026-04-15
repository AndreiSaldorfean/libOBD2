/* ================================================ INCLUDES =============================================== */
#include "init.h"
#include "libopencm3/cm3/nvic.h"
#include "libopencm3/stm32/f4/gpio.h"
#include <stdio.h>
#define STM32F4
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/f4/usart.h"
#include "tusb.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* ================================================= MACROS * ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
#if defined(SPT_FREERTOS)
SemaphoreHandle_t g_printMutex = NULL;
#endif /* SPT_FREERTOS */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void usbCdc_setup(void)
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
void sysInit()
{
    #if !defined(DEBUG)
    usbCdc_setup();
    setbuf(stdout, NULL); // disable buffering
    *(volatile uint32_t *)(0xe000edfc) |= 0x400;
#if defined(SPT_FREERTOS)
    g_printMutex = xSemaphoreCreateMutex();
#endif /* SPT_FREERTOS */
    #endif /* DEBUG */
}
