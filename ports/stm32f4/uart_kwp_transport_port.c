/* ================================================ INCLUDES =============================================== */
#include "l2_kwp2000.h"
#include "libobd2.h"
#include <stdio.h>
#define STM32F4
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/stm32/gpio.h"
#include "statusRetCodes.h"
#include "transport_if.h"
#include "uart_kwp_transport_port.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
bool UART_Init(void* handle)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    rcc_periph_clock_enable(ctx->gpioRcc);
    rcc_periph_clock_enable(ctx->usartClk);

    /* Configure USART BEFORE setting up GPIO pins */
    usart_set_baudrate(ctx->usartNum, ctx->baudRate);
    usart_set_databits(ctx->usartNum, ctx->dataBits);
    usart_set_stopbits(ctx->usartNum, ctx->stopBits);
    usart_set_mode(ctx->usartNum, ctx->mode);
    usart_set_parity(ctx->usartNum, ctx->parity);
    usart_set_flow_control(ctx->usartNum, ctx->flowControl);
    usart_enable(ctx->usartNum);

    /* Set ODR HIGH before any mode change (works even in input mode) */
    gpio_set(ctx->gpio, ctx->usartTxPin);

    /* Set alternate function BEFORE switching mode */
    gpio_set_af(ctx->gpio, GPIO_AF7, ctx->usartTxPin);
    gpio_set_af(ctx->gpio, GPIO_AF7, ctx->usartRxPin);

    /* Now switch to alternate function - USART already driving HIGH */
    gpio_mode_setup(ctx->gpio, GPIO_MODE_AF, GPIO_PUPD_NONE, ctx->usartTxPin);
    gpio_mode_setup(ctx->gpio, GPIO_MODE_AF, GPIO_PUPD_NONE, ctx->usartRxPin);

    gpio_set_output_options(ctx->gpio, ctx->gpioOutType, ctx->gpioOutSpeed ,ctx->usartTxPin);

    return 1;
}

void UART_WriteByte(void* handle, uint8_t data)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    YIELD;
    usart_send_blocking(ctx->usartNum, data);

    /* Wait for transmission to fully complete (shift register empty) */
    while (!(USART_SR(ctx->usartNum) & USART_SR_TC))
    {
        YIELD;
    }
}

bool UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    if((USART_SR(ctx->usartNum) & USART_SR_RXNE))
    {
        *recv_buffer = usart_recv(ctx->usartNum);
        return 1;
    }

    YIELD;

    return 0;
}

void UART_FlushRx(void* handle)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    /* Wait for previous send to finish */
    for (int i = 0; i < 1000; i++)
    {
        asm volatile ("nop");
    }

    /* STM32F4 USART has no FIFO - at most one byte can be pending in DR */
    while ((USART_SR(ctx->usartNum) & USART_SR_RXNE))
    {
        usart_recv(ctx->usartNum);
    }
}

void UART_SendPulse(void* handle, bool pulse)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;


    if (pulse)
    {
        gpio_set(ctx->gpio, ctx->usartTxPin);
    }
    else
    {
        gpio_clear(ctx->gpio, ctx->usartTxPin);
    }

    YIELD;
}

void UART_SwitchMode(void* handle, uint8_t mode)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    switch (mode)
    {
        case FAST_INIT_WAKEUP_START:
        case SLOW_INIT_5BAUD_START:
        {
            // Disable USART first
            usart_disable(ctx->usartNum);
            // Switch TX pin to GPIO output for bit-banging
            gpio_mode_setup(ctx->gpio, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ctx->usartTxPin);
            // Set line HIGH (idle state for K-line)
            gpio_set(ctx->gpio, ctx->usartTxPin);
            break;
        }
        case FAST_INIT_WAKEUP_END:
        case SLOW_INIT_5BAUD_END:
        {
            // Switch TX pin back to USART alternate function
            gpio_mode_setup(ctx->gpio, GPIO_MODE_AF, GPIO_PUPD_NONE, ctx->usartTxPin);
            gpio_set_af(ctx->gpio, GPIO_AF7, ctx->usartTxPin);
            // Re-enable USART
            usart_enable(ctx->usartNum);
            break;
        }
        default:
            break;
    }

}
