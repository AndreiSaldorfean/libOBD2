#include <stdint.h>
#define STM32F4
#include <stddef.h>
#include "string.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/cm3/systick.h"
#include "libopencm3/cm3/nvic.h"
#define GPIOC_MODE_REGISTER *(volatile uint32_t*)(uintptr_t)(0x40020800UL)
#define GPIOC_MODER13_SHIFT (26)
#define GPIOC_MODER13_MASK  (0xC000000)
#define GPIOC_OTYPE *(volatile uint32_t*)(uintptr_t)(0x40020800UL + 0x4)
#define GPIOC_ODR1 *(volatile uint32_t*)(uintptr_t)(0x40020800UL + 0x14U)

static void gpio_setup(void)
{
	/* Enable GPIOC clock. */
	/* Manually: */
	// RCC_AHB1ENR |= RCC_AHB1ENR_IOPDEN;
	/* Using API functions: */
	rcc_periph_clock_enable(RCC_GPIOC);

	/* Set GPIO13 (in GPIO port C) to 'output push-pull'. */
	/* Manually: */
	// GPIOD_CRH = (GPIO_CNF_OUTPUT_PUSHPULL << (((8 - 8) * 4) + 2));
	// GPIOD_CRH |= (GPIO_MODE_OUTPUT_2_MHZ << ((8 - 8) * 4));
	/* Using API functions: */
	// gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);

    uint32_t pinmode = (GPIOC_MODER13_MASK & (0x01<<GPIOC_MODER13_SHIFT));

    GPIOC_MODE_REGISTER |= pinmode;

    // GPIOC_OTYPE = 0;

}

static void usart_setup(void)
{
	{
		/* Enable clocks for USART1 and GPIOA. */
		rcc_periph_clock_enable(RCC_USART1);
		rcc_periph_clock_enable(RCC_GPIOA);

		/* Setup GPIO pins for USART1 transmit (PA9) and receive (PA10). */
		gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);
		gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);

		/* Setup USART1 parameters. */
		usart_set_baudrate(USART1, 9600);
		usart_set_databits(USART1, 8);
		usart_set_stopbits(USART1, USART_STOPBITS_1);
		usart_set_mode(USART1, USART_MODE_TX_RX);
		usart_set_parity(USART1, USART_PARITY_NONE);
		usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

		/* Finally enable the USART. */
		usart_enable(USART1);
	}

	{
		/* Enable clocks for USART1 and GPIOA. */
		rcc_periph_clock_enable(RCC_USART2);
		rcc_periph_clock_enable(RCC_GPIOA);

		/* Setup GPIO pins for USART1 transmit (PA2) and receive (PA3). */
		gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3);
		gpio_set_af(GPIOA, GPIO_AF7, GPIO2 | GPIO3);

		/* Setup USART1 parameters. */
		usart_set_baudrate(USART2, 9600);
		usart_set_databits(USART2, 8);
		usart_set_stopbits(USART2, USART_STOPBITS_1);
		usart_set_mode(USART2, USART_MODE_TX_RX);
		usart_set_parity(USART2, USART_PARITY_NONE);
		usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

		/* Finally enable the USART. */
		usart_enable(USART2);
	}
}

int main(void)
{
	int i;
	uint32_t counter = 0;

	gpio_setup();
	usart_setup();

	printf("STM32F401 USART1 initialized!\r\n");

	/* Blink the LED (PC8) on the board. */
	while (1) {
		/* Manually: */
		// GPIOD_BSRR = GPIO12;		/* LED off */
		// for (i = 0; i < 1000000; i++)	/* Wait a bit. */
		//	__asm__("nop");
		// GPIOD_BRR = GPIO12;		/* LED on */
		// for (i = 0; i < 1000000; i++)	/* Wait a bit. */
		//	__asm__("nop");

		/* Using API functions gpio_set()/gpio_clear(): */
		// gpio_set(GPIOD, GPIO12);	/* LED off */
		// for (i = 0; i < 1000000; i++)	/* Wait a bit. */
		//	__asm__("nop");
		// gpio_clear(GPIOD, GPIO12);	/* LED on */
		// for (i = 0; i < 1000000; i++)	/* Wait a bit. */
		//	__asm__("nop");

		/* Using API function gpio_toggle(): */
		// gpio_toggle(GPIOC, GPIO13);	/* LED on/off */
        GPIOC_ODR1 ^= (0x1 << 13);

		/* Send message via USART */
		printf("Counter: %lu\r\n", counter++);

		usart_send_blocking(USART2, (uint8_t)(counter & 0xFF));

		for (i = 0; i < 1000000; i++) {	/* Wait a bit. */
			__asm__("nop");
		}
	}
}
