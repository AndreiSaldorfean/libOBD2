/* ================================================ INCLUDES =============================================== */
#include "timer_neg_test.h"
#include "datalink.h"
#include "libobd2_timer_port.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TIME_STEP 10u

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* onOp fault hook: the operation stored in pUserData fails with a hardware error */
static void failOp(void *pUserData, virtTmrEvent_t *event)
{
    if (*(const virtTmrOp_t *)pUserData == event->op)
    {
        event->status = OBD_TMR_ERR_HW;
    }
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A failing timer init leaves the clock as it was
 * - description: The onOp fault hook makes TMR_Init fail. The error
 *                is returned and nothing in the context is reset.
 * - steps:
 *     1. Timer at 1234 ms with a step, init fails through the hook
 *     2. TMR_Init returns OBD_TMR_ERR_HW
 *     3. Time and step are unchanged
 ******************************************************************/
void test_neg_TMR_FaultHook_0(void)
{
    virtTmrOp_t failing = VIRT_TMR_OP_INIT;
    virtTmrCtx_t timer = { .timeNow = 1234u, .timeStep = TIME_STEP,
                           .faults = { .onOp = failOp, .pUserData = &failing } };

    /* 1. + 2. */
    TEST_ASSERT_EQUAL(OBD_TMR_ERR_HW, TMR_Init(&timer));

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(1234u, timer.timeNow);
    TEST_ASSERT_EQUAL_UINT32(TIME_STEP, timer.timeStep);
}

/******************************************************************
 * - brief:       TMR_Init rejects a NULL timer context
 * - description: Init validates its handle and reports an invalid
 *                argument instead of dereferencing NULL.
 * - steps:
 *     1. TMR_Init(NULL) returns OBD_TMR_ERR_INVALID_ARG
 ******************************************************************/
void test_neg_TMR_Init_0(void)
{
    UART_TRACE_Begin("test_neg_TMR_Init_0.vcd");
    TEST_ASSERT_EQUAL(OBD_TMR_ERR_INVALID_ARG, TMR_Init(NULL));
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A timeout that was never started never expires
 * - description: Polling IsTimeoutExpired without a StartTimeout, even
 *                long after a stale deadline, must not report expired.
 * - steps:
 *     1. Fresh timer with a stale deadline but no timeout started
 *     2. Move the clock far past that deadline
 *     3. Not expired
 ******************************************************************/
void test_neg_TMR_IsTimeoutExpired_0(void)
{
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP, .timeoutTime = 10u };

    /* 1. */
    TEST_ASSERT_FALSE(timer.timeoutStarted);

    /* 2. */
    TMR_DelayMs(&timer, 100000u);

    /* 3. */
    TEST_ASSERT_FALSE(TMR_IsTimeoutExpired(&timer));
}

/******************************************************************
 * - brief:       A failing start keeps the previous deadline
 * - description: When the timer hardware cannot start a timeout, the
 *                timeout that was already running must keep its old
 *                deadline instead of being moved or cleared.
 * - steps:
 *     1. Start P2 max at t=0 (deadline P2 max)
 *     2. At t=20 a restart fails with OBD_TMR_ERR_HW
 *     3. Deadline is still P2 max and the timeout still armed
 *     4. It expires at the old deadline
 ******************************************************************/
void test_neg_TMR_StartTimeout_0(void)
{
    virtTmrOp_t failing = VIRT_TMR_OP_COUNT;
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP,
                           .faults = { .onOp = failOp, .pUserData = &failing } };

    /* 1. */
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));

    /* 2. */
    TMR_DelayMs(&timer, 20u);
    failing = VIRT_TMR_OP_START_TIMEOUT;
    TEST_ASSERT_EQUAL(OBD_TMR_ERR_HW, TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(P2_TIME_MAX, timer.timeoutTime);
    TEST_ASSERT_TRUE(timer.timeoutStarted);

    /* 4. */
    TMR_DelayMs(&timer, P2_TIME_MAX - 20u + 1u);
    TEST_ASSERT_TRUE(TMR_IsTimeoutExpired(&timer));
}

/******************************************************************
 * - brief:       A failing stop leaves the timeout armed
 * - description: When StopTimeout fails, the caller gets the error and
 *                the timeout keeps running, so it still expires.
 * - steps:
 *     1. Start P1 max at t=0
 *     2. Stop fails with OBD_TMR_ERR_HW
 *     3. Timeout still armed and expired past its deadline
 ******************************************************************/
void test_neg_TMR_StopTimeout_0(void)
{
    virtTmrOp_t failing = VIRT_TMR_OP_STOP_TIMEOUT;
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP,
                           .faults = { .onOp = failOp, .pUserData = &failing } };

    /* 1. */
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, P1_TIME_MAX, NULL, NULL));

    /* 2. */
    TEST_ASSERT_EQUAL(OBD_TMR_ERR_HW, TMR_StopTimeout(&timer));

    /* 3. */
    TEST_ASSERT_TRUE(timer.timeoutStarted);
    TMR_DelayMs(&timer, P1_TIME_MAX + 1u);
    TEST_ASSERT_TRUE(TMR_IsTimeoutExpired(&timer));
}
