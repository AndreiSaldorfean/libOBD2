/* ================================================ INCLUDES =============================================== */
#include "uart_neg_test.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TIME_STEP 10u
#define NO_BYTE   0xA5u /* receive buffer marker: nothing was written into it */

/* ============================================ LOCAL VARIABLES ============================================ */
static virtTmrCtx_t timer;
static virtUartLinkCtx_t link;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* Fresh virtual line, no fault hooks */
static void setUpLink(void)
{
    timer = (virtTmrCtx_t){ .timeStep = TIME_STEP };
    link  = (virtUartLinkCtx_t){ .tmrHandle = &timer };
    TEST_ASSERT_TRUE(UART_Init(&link));
}

/* Bit-bangs 'bits' bits of a 0x33 frame (start, 8 data LSB first, stop) */
static void sendPulses(uint8_t bits)
{
    const uint16_t frame = (uint16_t)(0x200u | (0x33u << 1)); /* stop=1, data, start=0 */

    for (uint8_t bit = 0u; bit < bits; bit++)
    {
        UART_SendPulse(&link, (0u != ((frame >> bit) & 0x01u)));
    }
}

/* onRead fault hook: every byte handed to the receiver is an overrun */
static void injectOverrun(void *pUserData, uartEvent_t *event)
{
    (void)pUserData;
    event->error = OBD_UART_RX_OVERRUN_ERR;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       The onRead hook marks a received byte as overrun
 * - description: An overrun is reported with the byte, not instead
 *                of it: the byte is still delivered.
 * - steps:
 *     1. Install an onRead hook that sets OBD_UART_RX_OVERRUN_ERR
 *     2. Write 0x55, move to t=1
 *     3. Read returns OBD_UART_RX_OVERRUN_ERR together with 0x55
 ******************************************************************/
void test_neg_ex_UART_FaultHook_0(void)
{
    uint8_t received = 0u;

    UART_TRACE_Begin("test_neg_ex_UART_FaultHook_0.vcd");
    setUpLink();

    /* 1. */
    link.faults.onRead = injectOverrun;

    /* 2. */
    UART_WriteByte(&link, 0x55u);
    timer.timeNow = 1u;

    /* 3. */
    TEST_ASSERT_EQUAL(OBD_UART_RX_OVERRUN_ERR, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(0x55u, received);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       UART_Init rejects a NULL link context
 * - description: Init validates its handle and fails instead of
 *                dereferencing NULL.
 * - steps:
 *     1. UART_Init(NULL) returns false
 ******************************************************************/
void test_neg_ex_UART_Init_0(void)
{
    UART_TRACE_Begin("test_neg_ex_UART_Init_0.vcd");
    TEST_ASSERT_FALSE(UART_Init(NULL));
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       Reading an empty line reports no data
 * - description: With nothing on the line a read returns
 *                OBD_UART_RX_NO_DATA, leaves the buffer alone and
 *                still costs one poll step of virtual time, so a
 *                polling loop always makes progress.
 * - steps:
 *     1. Fresh line, nothing written
 *     2. Read returns OBD_UART_RX_NO_DATA, buffer untouched
 *     3. The clock moved one TIME_STEP
 ******************************************************************/
void test_neg_ex_UART_RecvByte_0(void)
{
    uint8_t received = NO_BYTE;

    UART_TRACE_Begin("test_neg_ex_UART_RecvByte_0.vcd");

    /* 1. */
    setUpLink();

    /* 2. */
    TEST_ASSERT_EQUAL(OBD_UART_RX_NO_DATA, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(NO_BYTE, received);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(TIME_STEP, timer.timeNow);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A byte is not visible at the time it was written
 * - description: The receiver only sees bytes written strictly before
 *                the current time: a read at the same ms finds
 *                nothing, the next poll gets the byte.
 * - steps:
 *     1. Write 0x7E at t=0
 *     2. Read at t=0 returns OBD_UART_RX_NO_DATA
 *     3. Read at t=TIME_STEP returns OK with 0x7E
 ******************************************************************/
void test_neg_ex_UART_RecvByte_1(void)
{
    uint8_t received = NO_BYTE;

    UART_TRACE_Begin("test_neg_ex_UART_RecvByte_1.vcd");
    setUpLink();

    /* 1. */
    UART_WriteByte(&link, 0x7Eu);

    /* 2. */
    TEST_ASSERT_EQUAL(OBD_UART_RX_NO_DATA, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(NO_BYTE, received);

    /* 3. */
    TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(0x7Eu, received);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       An incomplete bit-banged frame is dropped
 * - description: A frame that ends before its stop bit (9 of 10 bits)
 *                must not put a half-built byte on the line.
 * - steps:
 *     1. DIGITAL_MODE_BEGIN, start bit + 8 data bits, no stop bit
 *     2. DIGITAL_MODE_END
 *     3. The line stays empty
 ******************************************************************/
void test_neg_ex_UART_BitBang_0(void)
{
    uint8_t received = NO_BYTE;

    UART_TRACE_Begin("test_neg_ex_UART_BitBang_0.vcd");
    setUpLink();

    /* 1. */
    UART_SwitchMode(&link, DIGITAL_MODE_BEGIN);
    sendPulses(VIRT_UART_BITBANG_FRAME_BITS - 1u);

    /* 2. */
    UART_SwitchMode(&link, DIGITAL_MODE_END);

    /* 3. */
    timer.timeNow = 1u;
    TEST_ASSERT_EQUAL(OBD_UART_RX_NO_DATA, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(NO_BYTE, received);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       Pulses outside digital mode put nothing on the line
 * - description: While the UART owns the line (no DIGITAL_MODE_BEGIN)
 *                bit-banged pulses are ignored, even a complete frame,
 *                and a stray DIGITAL_MODE_END does not flush them.
 * - steps:
 *     1. Send a full 10-bit 0x33 frame without DIGITAL_MODE_BEGIN
 *     2. DIGITAL_MODE_END
 *     3. The line stays empty
 ******************************************************************/
void test_neg_ex_UART_BitBang_1(void)
{
    uint8_t received = NO_BYTE;

    UART_TRACE_Begin("test_neg_ex_UART_BitBang_1.vcd");
    setUpLink();

    /* 1. */
    sendPulses(VIRT_UART_BITBANG_FRAME_BITS);

    /* 2. */
    UART_SwitchMode(&link, DIGITAL_MODE_END);

    /* 3. */
    timer.timeNow = 1u;
    TEST_ASSERT_EQUAL(OBD_UART_RX_NO_DATA, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(NO_BYTE, received);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       UART_Init drops bytes left on the line
 * - description: Re-opening the line (e.g. a reconnect after an error)
 *                must not hand stale bytes of the old session to the
 *                next reader.
 * - steps:
 *     1. Write 0x01 0x02 0x03
 *     2. UART_Init again
 *     3. A read finds nothing
 ******************************************************************/
void test_neg_ex_UART_Init_1(void)
{
    uint8_t received = NO_BYTE;

    UART_TRACE_Begin("test_neg_ex_UART_Init_1.vcd");
    setUpLink();

    /* 1. */
    UART_WriteByte(&link, 0x01u);
    UART_WriteByte(&link, 0x02u);
    UART_WriteByte(&link, 0x03u);

    /* 2. */
    TEST_ASSERT_TRUE(UART_Init(&link));

    /* 3. */
    timer.timeNow = 1u;
    TEST_ASSERT_EQUAL(OBD_UART_RX_NO_DATA, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(NO_BYTE, received);
    UART_TRACE_End();
}
