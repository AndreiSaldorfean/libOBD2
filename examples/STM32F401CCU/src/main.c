#include "kwp_timer.h"
#include "uart_kwp_transport_port.h"
#include <stdint.h>
#define STM32F4
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/cm3/nvic.h"
#include "uart_kwp_transport_port.h"
#include "tusb.h"
#include "l2_kwp.h"

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

static void usbCdc_setup(void)
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
}

int main(void)
{
	int hello_sent = 0;
	uint32_t counter = 0;

	gpio_setup();
	usbCdc_setup();

    timerCtx_t tmrCtx =
    {
        .timeout_active = false,
        .timeout_expired = false,
        .timeout_callback = NULL,
        .timeout_user_data = NULL,
        .timeout_target_ms = 0,
    };

    KWP_TMR_Init(&tmrCtx);

    uartKwp_ctx_t uartCtx =
    {
            .usartClk    = RCC_USART1,
            .usartNum    = USART1,
            .baudRate    = 10400,
            .dataBits    = 8,
            .stopBits    = USART_STOPBITS_1,
            .mode        = USART_MODE_TX_RX,
            .parity      = USART_PARITY_NONE,
            .flowControl = USART_FLOWCONTROL_NONE,
            .usartTxPin  = GPIO9,
            .usartRxPin  = GPIO10,

            .gpioRcc     = RCC_GPIOA,
            .gpio        = GPIOA,
    };

    UART_KWP_Init(&uartCtx);

    UART_KWP_ChangeBaud(&uartCtx, FAST_INIT_WAKEUP_START);
    UART_KWP_SendPulse(&uartCtx, 0);

	/* Disable stdout buffering for immediate printf output */
	setbuf(stdout, NULL);

	/* Main loop - minimal config without RTOS */
	while (1) {
        GPIOC_ODR ^= (1 << 13);

        /* Send hello once when CDC is connected */
        if (!hello_sent) {
            // printf("USB CDC Connected!\r\n");
            // printf("Counter test: %lu\r\n", counter);
            hello_sent = 1;
		}

        UART_KWP_ChangeBaud(&uartCtx, FAST_INIT_WAKEUP_END);

        UART_KWP_WriteByte(&uartCtx, 0x83);

        for(int i=0;i<1000000;i++)
        {
            __asm__("nop");
        }
    }
}
