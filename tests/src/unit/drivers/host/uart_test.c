/* ================================================ INCLUDES =============================================== */
#include "uart_test.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include "theft.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern bool UART_Init(void* handle);
extern obd_uart_rx_status_t UART_RecvByte(void* handle, uint8_t *recv_buffer);
extern void UART_WriteByte(void* handle, uint8_t data);
extern void UART_FlushRx(void* handle);
extern void UART_SendPulse(void* handle, bool pulse);
extern void UART_SwitchMode(void* handle, uint8_t mode);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_UART_WriteByte_0(void)
{
    virtTmrCtx_t timer =
    {
        .timeNow  = 0u,
        .timeStep = 10u
    };
    virtUartLinkCtx_t ctx = {
        .tmrHandle      = &timer,
    };

    UART_TRACE_Begin("./test_UART_WriteByte_0.vcd");
    UART_TRACE_Note("ceva");

    UART_Init(&ctx);
    UART_WriteByte(&ctx, 0x49);   /* recorded at timeNow=0 */
    /* do NOT advance the timer — timeNow still equals the event timestamp */

    timer.timeNow = 1;

    uint8_t received = 0u;
    bool got_byte = (OBD_UART_RX_OK == UART_RecvByte(&ctx, &received));

    TEST_ASSERT_EQUAL_UINT32(0x49,received);
    UART_TRACE_End();
}

void test_UART_WriteByte_1(void)
{
    virtTmrCtx_t timer =
    {
        .timeNow  = 0u,
        .timeStep = 10u
    };
    virtUartLinkCtx_t ctx = {
        .tmrHandle      = &timer,
    };

    UART_TRACE_Begin("./test_UART_WriteByte_1.vcd");

    UART_Init(&ctx);

    uint8_t byte = 0x55;
    /* 5 baud: 200 ms per bit. The virtual timer counts whole ms, so 10400 baud (0.096 ms) can't be bit-banged */
    const uint16_t delay = (1000 / 5);

    // Set line HIGH (idle) and switch to bit-bang mode for 5 baud
    UART_SwitchMode(&ctx, DIGITAL_MODE_BEGIN);

    UART_SendPulse(&ctx, PULSE_LOW);
    TMR_DelayMs(&timer, 200);
    UART_SendPulse(&ctx, PULSE_HIGH);

    // Start bit (LOW)
    UART_SendPulse(&ctx, PULSE_LOW);
    TMR_DelayMs(&timer, delay);

    // Byte
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t bit = (byte >> i) & 0x01;
        UART_SendPulse(&ctx, bit ? PULSE_HIGH : PULSE_LOW);
        TMR_DelayMs(&timer, delay);
    }

    // Stop bit (HIGH)
    UART_SendPulse(&ctx, PULSE_HIGH);
    TMR_DelayMs(&timer, delay);

    // Switch to 10400 baud for response
    UART_SwitchMode(&ctx, DIGITAL_MODE_END);

    TMR_DelayMs(&timer, 100);
    UART_WriteByte(&ctx, 0x69);

    TMR_DelayMs(&timer, 300);
    UART_WriteByte(&ctx, 0x96);

    TMR_DelayMs(&timer, 300);
    UART_WriteByte(&ctx, 0x42);

    UART_TRACE_End();
}

/******************************************************************
 * A 5-baud bit-banged byte reaches the line.
 *   1. DIGITAL_MODE_BEGIN, then start bit, 0x33 LSB first, stop bit
 *   2. DIGITAL_MODE_END puts 0x33 on the line
 *   3. A later read returns it without error
 ******************************************************************/
void test_UART_BitBang_000(void)
{
    const uint8_t addr = 0x33u;
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = 10u };
    virtUartLinkCtx_t ctx = { .tmrHandle = &timer };
    uint8_t received = 0u;

    TEST_ASSERT_TRUE(UART_Init(&ctx));

    /* 1. */
    UART_SwitchMode(&ctx, DIGITAL_MODE_BEGIN);
    UART_SendPulse(&ctx, false);
    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
        UART_SendPulse(&ctx, (0u != ((addr >> bit) & 0x01u)));
    }
    UART_SendPulse(&ctx, true);

    /* 2. */
    UART_SwitchMode(&ctx, DIGITAL_MODE_END);

    /* 3. */
    timer.timeNow = 1u;
    TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&ctx, &received));
    TEST_ASSERT_EQUAL_HEX8(addr, received);
}
