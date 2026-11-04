/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#define STM32F4
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/stm32/gpio.h"
#include "statusRetCodes.h"
#include "uart_if.h"
#include "utils.h"
#include "libobd2_uart_port.h"
#include "init.h"
#include "trace.h"

/* ================================================= MACROS ================================================ */
#define UART_GPIO_AF             (GPIO_AF7) /* alternate function of the USART TX/RX pins */
#define UART_FLUSH_SETTLE_LOOPS  (1000)     /* busy loops to let the last sent byte reach RX */
#define UART_RX_ERROR_FLAGS      (USART_SR_ORE | USART_SR_FE | USART_SR_PE | USART_SR_NE)

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static obd_uart_rx_status_t UART_RxErrorFromStatus(uint32_t sr);

static bool UART_Init_Validate(const uart_ctx_t *ctx);
static void UART_Init_Execute(const uart_ctx_t *ctx);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: UART_RxErrorFromStatus
* brief: Maps the USART_SR error flags of a received byte to its rx status
*        (overrun first, then framing, parity, noise).
******************************************/
static obd_uart_rx_status_t UART_RxErrorFromStatus(uint32_t sr)
{
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_OK;

    if (0U != (sr & USART_SR_ORE))
    {
        rxStatus = OBD_UART_RX_OVERRUN_ERR;
    }
    else if (0U != (sr & USART_SR_FE))
    {
        rxStatus = OBD_UART_RX_FRAMING_ERR;
    }
    else if (0U != (sr & USART_SR_PE))
    {
        rxStatus = OBD_UART_RX_PARITY_ERR;
    }
    else if (0U != (sr & USART_SR_NE))
    {
        rxStatus = OBD_UART_RX_NOISE_ERR;
    }

    return rxStatus;
}

/******************************************
* title: UART_Init_Validate
* brief: Checks the arguments of UART_Init.
******************************************/
static bool UART_Init_Validate(const uart_ctx_t *ctx)
{
    return (NULL != ctx);
}

/******************************************
* title: UART_Init_Execute
* brief: Configures the USART, then hands the TX/RX pins to it with the line
*        already idle (high).
******************************************/
static void UART_Init_Execute(const uart_ctx_t *ctx)
{
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
    gpio_set_af(ctx->gpio, UART_GPIO_AF, ctx->usartTxPin);
    gpio_set_af(ctx->gpio, UART_GPIO_AF, ctx->usartRxPin);

    /* Now switch to alternate function - USART already driving HIGH */
    gpio_mode_setup(ctx->gpio, GPIO_MODE_AF, GPIO_PUPD_NONE, ctx->usartTxPin);
    gpio_mode_setup(ctx->gpio, GPIO_MODE_AF, GPIO_PUPD_NONE, ctx->usartRxPin);

    gpio_set_output_options(ctx->gpio, ctx->gpioOutType, ctx->gpioOutSpeed ,ctx->usartTxPin);

    trace_init();
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: UART_Init
* brief: Sets up the USART and its pins for K-line communication.
******************************************/
bool UART_Init(void* handle)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;
    bool ok = false;

    ok = UART_Init_Validate(ctx);
    OBD2_IF_COND_GOTO_EXIT(!ok);
    UART_Init_Execute(ctx);

exit:
    return ok;
}

/******************************************
* title: UART_WriteByte
* brief: Sends one byte and waits until it has left the shift register,
*        yielding while it does.
******************************************/
void UART_WriteByte(void* handle, uint8_t data)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

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

/******************************************
* title: UART_RecvByte
* brief: Reads the pending byte, if any, and reports the USART error it came
*        with. Reading SR then DR clears the error flags.
******************************************/
obd_uart_rx_status_t UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_NO_DATA;
    uint32_t sr = 0;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != recv_buffer));

    sr = USART_SR(ctx->usartNum);

    if (0U != (sr & (USART_SR_RXNE | UART_RX_ERROR_FLAGS)))
    {
        *recv_buffer = (uint8_t)usart_recv(ctx->usartNum);
        rxStatus     = UART_RxErrorFromStatus(sr);
        TRACE_LOG(TRACE_TAG_RX, *recv_buffer, ctx->usartNum);
    }
    else
    {
        YIELD;
    }

    return rxStatus;
}

/******************************************
* title: UART_FlushRx
* brief: Lets the last sent byte come back, then drops whatever is pending in
*        the receive register.
******************************************/
void UART_FlushRx(void* handle)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    TRACE_LOG(TRACE_TAG_FLUSH, 0x00, ctx->usartNum);

    /* Wait for previous send to finish */
    for (int i = 0; i < UART_FLUSH_SETTLE_LOOPS; i++)
    {
        __asm__ volatile ("nop");
    }

    /* STM32F4 USART has no FIFO - at most one byte can be pending in DR */
    while ((USART_SR(ctx->usartNum) & USART_SR_RXNE))
    {
        usart_recv(ctx->usartNum);
    }
}

/******************************************
* title: UART_SendPulse
* brief: Drives the TX pin high or low while it is in a bit-bang mode.
******************************************/
void UART_SendPulse(void* handle, bool pulse)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

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

/******************************************
* title: UART_SwitchMode
* brief: Hands the TX pin to GPIO for bit-banging (*_BEGIN / *_START) or
*        back to the USART (*_END).
******************************************/
void UART_SwitchMode(void* handle, uint8_t mode)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

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
            gpio_set_af(ctx->gpio, UART_GPIO_AF, ctx->usartTxPin);
            // Re-enable USART
            usart_enable(ctx->usartNum);
            break;
        }
        default:
            break;
    }
}
