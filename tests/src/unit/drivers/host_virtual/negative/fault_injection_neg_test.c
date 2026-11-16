/* ================================================ INCLUDES =============================================== */
#include "fault_injection_neg_test.h"
#include "fault_injection.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TIME_STEP 10u

/* ============================================ LOCAL VARIABLES ============================================ */
static virtTmrCtx_t timer;
static virtUartLinkCtx_t link;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* Fresh virtual line, no injector */
static void setUpLink(void)
{
    timer = (virtTmrCtx_t){ .timeStep = TIME_STEP };
    link  = (virtUartLinkCtx_t){ .tmrHandle = &timer };
    TEST_ASSERT_TRUE(UART_Init(&link));
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       Attach rejects NULL arguments and mismatched rules
 * - description: Timer rules whose effect does not fit their operation
 *                (FAIL on get_time, EXPIRE on delay, SHIFT on
 *                start_timeout, no effect at all) are refused, and
 *                nothing is installed on the link or the timer.
 * - steps:
 *     1. NULL link / NULL injector / NULL timer are refused
 *     2. Every mismatched timer rule is refused
 *     3. No hook is installed
 ******************************************************************/
void test_neg_FAULT_Attach_0(void)
{
    faultUartInjector_t uartInj = {0};
    faultTmrInjector_t tmrInj = { .rule = { .op = VIRT_TMR_OP_START_TIMEOUT, .effect = FAULT_TMR_FAIL } };

    UART_TRACE_Begin("test_neg_FAULT_Attach_0.vcd");
    setUpLink();

    /* 1. */
    TEST_ASSERT_FALSE(FAULT_UartAttach(NULL, &uartInj));
    TEST_ASSERT_FALSE(FAULT_UartAttach(&link, NULL));
    TEST_ASSERT_FALSE(FAULT_TmrAttach(NULL, &tmrInj));
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, NULL));

    /* 2. */
    tmrInj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_GET_TIME, .effect = FAULT_TMR_FAIL };
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &tmrInj));
    tmrInj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_DELAY, .effect = FAULT_TMR_EXPIRE };
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &tmrInj));
    tmrInj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_START_TIMEOUT, .effect = FAULT_TMR_SHIFT };
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &tmrInj));
    tmrInj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_INIT };
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &tmrInj));

    /* 3. */
    TEST_ASSERT_NULL(link.faults.onWrite);
    TEST_ASSERT_NULL(timer.faults.onOp);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A refused attach keeps the injector already in place
 * - description: Swapping in an invalid timer rule must not remove or
 *                reset the injector that is attached: the old rule
 *                keeps firing and its counters keep counting.
 * - steps:
 *     1. Attach a valid FAIL rule on start_timeout; one call fails
 *     2. Try to attach a second injector with a mismatched rule: refused
 *     3. The first injector is still installed: the next start fails
 *        too and its counter reaches 2
 ******************************************************************/
void test_neg_FAULT_Tmr_0(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_START_TIMEOUT, .effect = FAULT_TMR_FAIL } };
    faultTmrInjector_t bad = { .rule = { .op = VIRT_TMR_OP_DELAY, .effect = FAULT_TMR_FAIL } };

    UART_TRACE_Begin("test_neg_FAULT_Tmr_0.vcd");
    setUpLink();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));
    TEST_ASSERT_EQUAL(OBD_TMR_ERR_HW, TMR_StartTimeout(&timer, 10u, NULL, NULL));

    /* 2. */
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &bad));

    /* 3. */
    TEST_ASSERT_EQUAL_PTR(&inj, timer.faults.pUserData);
    TEST_ASSERT_EQUAL(OBD_TMR_ERR_HW, TMR_StartTimeout(&timer, 10u, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT32(2u, inj.injected);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A UART rule whose window is never reached does nothing
 * - description: A rule aimed at byte 10 of a 3-byte exchange must not
 *                touch any byte, and reports 0 injections.
 * - steps:
 *     1. Attach an onWrite parity rule for byte 10
 *     2. Write and read 3 bytes
 *     3. All reads OK with the bytes as written, 0 injected
 ******************************************************************/
void test_neg_FAULT_Uart_0(void)
{
    faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_PARITY_ERR, .xorMask = 0x80u, .firstByte = 10u, .count = 1u } };
    uint8_t received = 0u;

    UART_TRACE_Begin("test_neg_FAULT_Uart_0.vcd");
    setUpLink();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_UartAttach(&link, &inj));

    /* 2. + 3. */
    for (uint8_t idx = 0u; idx < 3u; idx++)
    {
        timer.timeNow = idx;
        UART_WriteByte(&link, (uint8_t)(0x40u + idx));
    }
    timer.timeNow = 3u;
    for (uint8_t idx = 0u; idx < 3u; idx++)
    {
        TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&link, &received));
        TEST_ASSERT_EQUAL_HEX8(0x40u + idx, received);
    }
    TEST_ASSERT_EQUAL_UINT32(0u, inj.injected);
    TEST_ASSERT_EQUAL_UINT32(3u, inj.written);
    UART_TRACE_End();
}
