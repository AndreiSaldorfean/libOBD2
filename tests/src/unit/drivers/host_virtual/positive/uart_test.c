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

/* ================================================= MACROS ================================================ */
#define BAUD_5_BIT_TIME (1000u / 5u) /* 200 ms per bit at 5 baud */

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A written byte can be read back from the line
 * - description: The host virtual UART is a loopback: a byte written
 *                at t=0 is visible to a read from t=1 on.
 * - steps:
 *     1. Write 0x49 at t=0
 *     2. Move the clock to t=1
 *     3. Read returns OK with 0x49
 ******************************************************************/
void test_pos_UART_WriteByte_0(void)
{
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = 10u };
    virtUartLinkCtx_t ctx = { .tmrHandle = &timer };
    uint8_t received = 0u;

    UART_TRACE_Begin("test_pos_UART_WriteByte_0.vcd");
    TEST_ASSERT_TRUE(UART_Init(&ctx));

    /* 1. */
    UART_WriteByte(&ctx, 0x49u);

    /* 2. */
    timer.timeNow = 1u;

    /* 3. */
    TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&ctx, &received));
    TEST_ASSERT_EQUAL_HEX8(0x49u, received);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       5-baud bit-bang followed by 10400 baud bytes
 * - description: Draws a full wake-up sequence on the trace: a 0x55
 *                byte bit-banged at 5 baud, then three bytes at
 *                10400 baud. Mainly a trace showcase for PulseView
 *                (both decoder rows are filled).
 * - steps:
 *     1. DIGITAL_MODE_BEGIN, a 200 ms low/high pre-pulse
 *     2. Start bit, 0x55 LSB first, stop bit, 200 ms each
 *     3. DIGITAL_MODE_END, then 0x69, 0x96, 0x42 with gaps
 *     4. Clock moved by every delay: 200 + 10 * 200 + 700 ms
 ******************************************************************/
void test_pos_UART_WriteByte_1(void)
{
    const uint8_t byte = 0x55u;
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = 10u };
    virtUartLinkCtx_t ctx = { .tmrHandle = &timer };

    UART_TRACE_Begin("test_pos_UART_WriteByte_1.vcd");
    TEST_ASSERT_TRUE(UART_Init(&ctx));

    /* 1. */
    UART_SwitchMode(&ctx, DIGITAL_MODE_BEGIN);
    UART_SendPulse(&ctx, PULSE_LOW);
    TMR_DelayMs(&timer, BAUD_5_BIT_TIME);
    UART_SendPulse(&ctx, PULSE_HIGH);

    /* 2. */
    UART_SendPulse(&ctx, PULSE_LOW);
    TMR_DelayMs(&timer, BAUD_5_BIT_TIME);
    for (uint8_t i = 0; i < 8u; i++)
    {
        UART_SendPulse(&ctx, (0u != ((byte >> i) & 0x01u)) ? PULSE_HIGH : PULSE_LOW);
        TMR_DelayMs(&timer, BAUD_5_BIT_TIME);
    }
    UART_SendPulse(&ctx, PULSE_HIGH);
    TMR_DelayMs(&timer, BAUD_5_BIT_TIME);

    /* 3. */
    UART_SwitchMode(&ctx, DIGITAL_MODE_END);
    TMR_DelayMs(&timer, 100u);
    UART_WriteByte(&ctx, 0x69u);
    TMR_DelayMs(&timer, 300u);
    UART_WriteByte(&ctx, 0x96u);
    TMR_DelayMs(&timer, 300u);
    UART_WriteByte(&ctx, 0x42u);
    UART_TRACE_End();

    /* 4. */
    TEST_ASSERT_EQUAL_UINT32(BAUD_5_BIT_TIME + 10u * BAUD_5_BIT_TIME + 700u, timer.timeNow);
}

/******************************************************************
 * - brief:       A 5-baud bit-banged byte reaches the line
 * - description: The virtual UART collects the bit-banged pulses and
 *                puts the complete byte on the line at mode end.
 * - steps:
 *     1. DIGITAL_MODE_BEGIN, then start bit, 0x33 LSB first, stop bit
 *     2. DIGITAL_MODE_END puts 0x33 on the line
 *     3. A later read returns it without error
 ******************************************************************/
void test_pos_UART_BitBang_0(void)
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
