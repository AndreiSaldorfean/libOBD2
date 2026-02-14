/* ================================================ INCLUDES =============================================== */
#include "l2_kwp.h"
#define STM32F4
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/stm32/gpio.h"
#include "srv_status.h"
#include "transport_if.h"
#include "uart_kwp_transport_port.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
obd_status_t UART_KWP_Init(void* handle)
{
    uartKwp_ctx_t *ctx = (uartKwp_ctx_t*)handle;

    rcc_periph_clock_enable(ctx->gpioRcc);
    rcc_periph_clock_enable(ctx->usartClk);

    gpio_mode_setup(ctx->gpio, GPIO_MODE_AF, GPIO_PUPD_NONE, ctx->usartTxPin);
    gpio_mode_setup(ctx->gpio, GPIO_MODE_AF, GPIO_PUPD_NONE, ctx->usartRxPin);

    gpio_set_af(ctx->gpio, GPIO_AF7, ctx->usartTxPin);
    gpio_set_af(ctx->gpio, GPIO_AF7, ctx->usartRxPin);

    /* Usart setup for kline comm */
    {
        /* Setup usart parameters. */
        usart_set_baudrate(ctx->usartNum, ctx->baudRate);
        usart_set_databits(ctx->usartNum, ctx->dataBits);
        usart_set_stopbits(ctx->usartNum, ctx->stopBits);
        usart_set_mode(ctx->usartNum, ctx->mode);
        usart_set_parity(ctx->usartNum, ctx->parity);
        usart_set_flow_control(ctx->usartNum, ctx->flowControl);

        /* Finally enable the usart. */
        usart_enable(ctx->usartNum);
    }

    return OBD_STATUS_OK;
}

obd_status_t UART_KWP_WriteByte(void* handle, uint8_t data)
{
    uartKwp_ctx_t *ctx = (uartKwp_ctx_t*)handle;

    usart_send_blocking(ctx->usartNum, data);

    return OBD_STATUS_OK;
}

obd_status_t UART_KWP_RecvByte(void* handle, uint8_t *recv_buffer)
{
    uartKwp_ctx_t *ctx = (uartKwp_ctx_t*)handle;

    *recv_buffer = usart_recv_blocking(ctx->usartNum);

    return OBD_STATUS_OK;
}

obd_status_t UART_KWP_SendPulse(void* handle, bool pulse)
{
    uartKwp_ctx_t *ctx = (uartKwp_ctx_t*)handle;

    if (pulse)
    {
        gpio_set(ctx->gpio, ctx->usartTxPin);
    }
    else
    {
        gpio_clear(ctx->gpio, ctx->usartTxPin);
    }

    return OBD_STATUS_OK;
}

void UART_KWP_ChangeBaud(void* handle, uint8_t mode)
{
    uartKwp_ctx_t *ctx = (uartKwp_ctx_t*)handle;

    switch (mode)
    {
        case FAST_INIT_WAKEUP_START:
        {
            // Disable USART first
            usart_disable(ctx->usartNum);
            // Switch TX pin to GPIO output for bit-banging
            gpio_mode_setup(ctx->gpio, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ctx->usartTxPin);
            break;
        }
        case FAST_INIT_WAKEUP_END:
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
