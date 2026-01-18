#include <stdint.h>
#define STM32F4
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/cm3/nvic.h"
#include "tusb.h"

#define GPIOC_MODE_REGISTER *(volatile uint32_t*)(uintptr_t)(0x40020800UL)
#define GPIOC_MODER13_SHIFT (26)
#define GPIOC_MODER13_MASK  (0xC000000)
#define GPIOC_OTYPE *(volatile uint32_t*)(uintptr_t)(0x40020800UL + 0x4)

static void gpio_setup(void)
{
	rcc_periph_clock_enable(RCC_GPIOC);

    uint32_t pinmode = (GPIOC_MODER13_MASK & (0x01<<GPIOC_MODER13_SHIFT));

    GPIOC_MODE_REGISTER |= pinmode;
}

static void usart_setup(void)
{
    /* Use internal HSI oscillator - works on all F401CCU boards without crystal */
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    /* Enable GPIO clocks for USB */
    rcc_periph_clock_enable(RCC_GPIOA);

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

int main(void)
{
	int hello_sent = 0;
	uint32_t counter = 0;

	gpio_setup();
	usart_setup();

	/* Disable stdout buffering for immediate printf output */
	setbuf(stdout, NULL);

	/* Main loop - minimal config without RTOS */
	while (1) {

        GPIOC_ODR ^= (1 << 13);

        /* Send hello once when CDC is connected */
        if (!hello_sent) {
            printf("USB CDC Connected!\r\n");
            printf("Counter test: %lu\r\n", counter);
            hello_sent = 1;
		}

        for(int i=0;i<1000000;i++)
        {
            __asm__("nop");
        }
    }
}
