/* ================================================ INCLUDES =============================================== */
#include "l2_kwp2000.h"
#include "libobd2.h"
#include <stdio.h>
#define STM32F4
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/stm32/gpio.h"
#include "statusRetCodes.h"
#include "uart_if.h"
#include "libobd2_uart_port.h"
#include "init.h"
#include "trace.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
bool LIBOBD2_UART_Init(void* handle)
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

    trace_init();

    return 1;
}

void LIBOBD2_UART_WriteByte(void* handle, uint8_t data)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    usart_send_blocking(ctx->usartNum, data);
    TRACE_LOG(TRACE_TAG_TX, data, ctx->usartNum);

    /* Yield on every poll so the receiver task gets CPU while the byte
     * shifts out (~960 µs at 10400 baud).  Without this the sender holds
     * the CPU for a full baud period and the receiver can miss the RXNE
     * window, causing an overrun on the next byte. */
    while (!(USART_SR(ctx->usartNum) & USART_SR_TC))
    {
        YIELD;
    }
}

bool LIBOBD2_UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    if((USART_SR(ctx->usartNum) & USART_SR_RXNE))
    {
        *recv_buffer = usart_recv(ctx->usartNum);
        TRACE_LOG(TRACE_TAG_RX, *recv_buffer, ctx->usartNum);
        return 1;
    }

    YIELD;
    return 0;
}

void LIBOBD2_UART_FlushRx(void* handle)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    TRACE_LOG(TRACE_TAG_FLUSH, 0x00, ctx->usartNum);

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

void LIBOBD2_UART_SendPulse(void* handle, bool pulse)
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

void LIBOBD2_UART_SwitchMode(void* handle, uint8_t mode)
{
    uart_ctx_t *ctx = (uart_ctx_t*)handle;

    switch (mode)
    {
        case FAST_INIT_WAKEUP_START:
        case DIGITAL_MODE_BEGIN:
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
        case DIGITAL_MODE_END:
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
