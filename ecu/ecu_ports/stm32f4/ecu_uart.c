/* ================================================ INCLUDES =============================================== */
#include "ecu_uart.h"
#include "datalink.h"
#include "l2_kwp2000.h"
#include "libobd2.h"
#include <stdio.h>
#define STM32F4
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/stm32/gpio.h"
#include "transport_if.h"
#include "uart_kwp_transport_port.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static inline void ECUSIM_DisableUart(dataLink_if_t *self);
static inline void ECUSIM_EnableUart(dataLink_if_t *self);
static inline bool ECUSIM_ReadBit(dataLink_if_t * self);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static inline void ECUSIM_DisableUart(dataLink_if_t *self)
{
    uart_ctx_t *ctx = (uart_ctx_t*)(self->pTransportHandle);

    // Disable USART first
    usart_disable(ctx->usartNum);
    // Switch RX pin to GPIO output for bit-banging
    gpio_mode_setup(ctx->gpio, GPIO_MODE_INPUT, GPIO_PUPD_NONE, ctx->usartRxPin);
}

static inline void ECUSIM_EnableUart(dataLink_if_t *self)
{
    uart_ctx_t *ctx = (uart_ctx_t*)(self->pTransportHandle);

    // Switch TX pin back to USART alternate function
    gpio_mode_setup(ctx->gpio, GPIO_MODE_AF, GPIO_PUPD_NONE, ctx->usartRxPin);
    gpio_set_af(ctx->gpio, GPIO_AF7, ctx->usartRxPin);

    // Re-enable USART
    usart_enable(ctx->usartNum);

    if (USART_SR(ctx->usartNum) & USART_SR_RXNE)
        (void)usart_recv(ctx->usartNum);
}

static inline bool ECUSIM_ReadBit(dataLink_if_t * self)
{
    uart_ctx_t *ctx = (uart_ctx_t*)(self->pTransportHandle);
    bool val = 0;

    val = gpio_get(ctx->gpio, ctx->usartRxPin) != 0;
    YIELD;

    return val;
}

/* ================================================ MODULE API ============================================= */
bool ECUSIM_ReadByteBitBanged(dataLink_if_t *self, uint8_t *byte, uint8_t baudRate)
{
    const uint16_t bit_period = 1000u / baudRate; // ms per bit (200 ms at 5 baud)

    *byte = 0;
    ECUSIM_DisableUart(self);

    while (ECUSIM_ReadBit(self) == 1);

    LIBOBD_Delay(self, bit_period + (bit_period / 2u));

    for (int bitIdx = 0; bitIdx < 8; bitIdx++)
    {
        *byte |= (uint8_t)(ECUSIM_ReadBit(self) << bitIdx);
        LIBOBD_Delay(self, bit_period);
    }

    if (ECUSIM_ReadBit(self) != 1)
    {
        ECUSIM_EnableUart(self);
        return false;
    }
    LIBOBD_Delay(self, bit_period);

    ECUSIM_EnableUart(self);

    return true;
}

