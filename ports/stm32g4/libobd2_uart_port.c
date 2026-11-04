/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "libopencm3/stm32/g4/rcc.h"
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
#define UART_RX_ERROR_FLAGS      (USART_ISR_ORE | USART_ISR_FE | USART_ISR_PE | USART_ISR_NF)
#define UART_RX_ERROR_CLEAR      (USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_PECF | USART_ICR_NCF)

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static obd_uart_rx_status_t UART_RxErrorFromStatus(uint32_t isr);

static bool UART_Init_Validate(const uart_ctx_t *ctx);
static void UART_Init_Execute(const uart_ctx_t *ctx);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: UART_RxErrorFromStatus
* brief: Maps the USART_ISR error flags of a received byte to its rx status
*        (overrun first, then framing, parity, noise).
******************************************/
static obd_uart_rx_status_t UART_RxErrorFromStatus(uint32_t isr)
{
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_OK;

    if (0U != (isr & USART_ISR_ORE))
    {
        rxStatus = OBD_UART_RX_OVERRUN_ERR;
    }
    else if (0U != (isr & USART_ISR_FE))
    {
        rxStatus = OBD_UART_RX_FRAMING_ERR;
    }
    else if (0U != (isr & USART_ISR_PE))
    {
        rxStatus = OBD_UART_RX_PARITY_ERR;
    }
    else if (0U != (isr & USART_ISR_NF))
    {
        rxStatus = OBD_UART_RX_NOISE_ERR;
    }

    return rxStatus;
}

/******************************************
* title: UART_Init_Validate
* brief: Checks the arguments of LIBOBD2_UART_Init.
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
* title: LIBOBD2_UART_Init
* brief: Sets up the USART and its pins for K-line communication.
******************************************/
bool LIBOBD2_UART_Init(void* handle)
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
* title: LIBOBD2_UART_WriteByte
* brief: Sends one byte and waits until it has left the shift register,
*        yielding while it does.
******************************************/
void LIBOBD2_UART_WriteByte(void* handle, uint8_t data)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    usart_send_blocking(ctx->usartNum, data);
    TRACE_LOG(TRACE_TAG_TX, data, ctx->usartNum);

    /* Yield on every poll so the receiver task gets CPU while the byte
     * shifts out (~960 µs at 10400 baud).  Without this the sender holds
     * the CPU for a full baud period and the receiver can miss the RXNE
     * window, causing an overrun on the next byte. */
    while (!(USART_ISR(ctx->usartNum) & USART_ISR_TC))
    {
        YIELD;
    }
}

/******************************************
* title: LIBOBD2_UART_RecvByte
* brief: Reads the pending byte, if any, and reports the USART error it came
*        with. The error flags are cleared through ICR.
******************************************/
obd_uart_rx_status_t LIBOBD2_UART_RecvByte(void* handle, uint8_t *recv_buffer)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_NO_DATA;
    uint32_t isr = 0;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != recv_buffer));

    isr = USART_ISR(ctx->usartNum);

    if (0U != (isr & (USART_ISR_RXNE | UART_RX_ERROR_FLAGS)))
    {
        *recv_buffer = (uint8_t)usart_recv(ctx->usartNum);
        rxStatus     = UART_RxErrorFromStatus(isr);
        USART_ICR(ctx->usartNum) = UART_RX_ERROR_CLEAR;
        TRACE_LOG(TRACE_TAG_RX, *recv_buffer, ctx->usartNum);
    }
    else
    {
        YIELD;
    }

    return rxStatus;
}

/******************************************
* title: LIBOBD2_UART_FlushRx
* brief: Lets the last sent byte come back, drops whatever is pending in the
*        receive register and clears the error flags the echo left behind.
******************************************/
void LIBOBD2_UART_FlushRx(void* handle)
{
    uart_ctx_t *ctx = (uart_ctx_t *)handle;

    OBD2_DEBUG_ASSERT(NULL != ctx);

    TRACE_LOG(TRACE_TAG_FLUSH, 0x00, ctx->usartNum);

    /* Wait for previous send to finish */
    for (int i = 0; i < UART_FLUSH_SETTLE_LOOPS; i++)
    {
        __asm__ volatile ("nop");
    }

    /* With the FIFO disabled at most one byte is pending in RDR */
    while ((USART_ISR(ctx->usartNum) & USART_ISR_RXNE))
    {
        usart_recv(ctx->usartNum);
    }

    /* An echo that overran RDR leaves ORE set, which would block reception */
    USART_ICR(ctx->usartNum) = UART_RX_ERROR_CLEAR;
}

/******************************************
* title: LIBOBD2_UART_SendPulse
* brief: Drives the TX pin high or low while it is in a bit-bang mode.
******************************************/
void LIBOBD2_UART_SendPulse(void* handle, bool pulse)
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
* title: LIBOBD2_UART_SwitchMode
* brief: Hands the TX pin to GPIO for bit-banging (*_BEGIN / *_START) or
*        back to the USART (*_END).
******************************************/
void LIBOBD2_UART_SwitchMode(void* handle, uint8_t mode)
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
