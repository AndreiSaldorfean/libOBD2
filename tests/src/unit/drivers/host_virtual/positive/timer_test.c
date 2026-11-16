/* ================================================ INCLUDES =============================================== */
#include "timer_test.h"
#include "datalink.h"
#include "iso9141.h"
#include "libobd2_timer_port.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TIME_STEP 10u

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* onOp fault hook: every timeout reports expired at once */
static void forceTimeoutExpired(void *pUserData, virtTmrEvent_t *event)
{
    (void)pUserData;
    if (VIRT_TMR_OP_IS_TIMEOUT_EXPIRED == event->op)
    {
        event->expired = true;
    }
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       TMR_Init resets the virtual clock
 * - description: Init puts the virtual clock back to a known state:
 *                time, step and timeout are all cleared.
 * - steps:
 *     1. Start from a timer with time, step and a running timeout
 *     2. TMR_Init returns OK
 *     3. Time, step and timeout are all 0 / stopped
 ******************************************************************/
void test_pos_TMR_Init_0(void)
{
    virtTmrCtx_t timer =
    {
        .timeNow        = 1234u,
        .timeStep       = TIME_STEP,
        .timeoutTime    = 5678u,
        .timeoutStarted = true,
    };

    /* 1. + 2. */
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_Init(&timer));

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeNow);
    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeStep);
    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeoutTime);
    TEST_ASSERT_FALSE(timer.timeoutStarted);
}

/******************************************************************
 * - brief:       TMR_GetTimeMs reads the clock without moving it
 * - description: Reading the time twice gives the same value and
 *                leaves timeNow untouched.
 * - steps:
 *     1. Timer at 2360 ms
 *     2. Two reads both return 2360, timeNow is still 2360
 ******************************************************************/
void test_pos_TMR_GetTimeMs_0(void)
{
    virtTmrCtx_t timer = { .timeNow = 2360u, .timeStep = TIME_STEP };

    TEST_ASSERT_EQUAL_UINT32(2360u, TMR_GetTimeMs(&timer));
    TEST_ASSERT_EQUAL_UINT32(2360u, TMR_GetTimeMs(&timer));
    TEST_ASSERT_EQUAL_UINT32(2360u, timer.timeNow);
}

/******************************************************************
 * - brief:       TMR_DelayMs moves the clock by exactly the delay
 * - description: One ISO9141 request worth of delays: P3 idle, then
 *                P4 after each of 6 bytes. A 0 ms delay is a no-op,
 *                and the running timeout is left alone.
 * - steps:
 *     1. Arm a P2 max timeout
 *     2. Delay P3 min -> clock at P3 min
 *     3. Delay P4 min 6 times -> clock at P3 + 6 * P4
 *     4. Delay 0 ms -> clock unchanged
 *     5. Step and timeout are untouched
 ******************************************************************/
void test_pos_TMR_DelayMs_0(void)
{
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP };

    /* 1. */
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));

    /* 2. */
    TMR_DelayMs(&timer, P3_TIME_MIN);
    TEST_ASSERT_EQUAL_UINT32(P3_TIME_MIN, TMR_GetTimeMs(&timer));

    /* 3. */
    for (uint8_t idx = 0; idx < 6u; idx++)
    {
        TMR_DelayMs(&timer, P4_TIME_MIN);
    }
    TEST_ASSERT_EQUAL_UINT32(P3_TIME_MIN + 6u * P4_TIME_MIN, TMR_GetTimeMs(&timer));

    /* 4. */
    TMR_DelayMs(&timer, 0u);
    TEST_ASSERT_EQUAL_UINT32(P3_TIME_MIN + 6u * P4_TIME_MIN, TMR_GetTimeMs(&timer));

    /* 5. */
    TEST_ASSERT_EQUAL_UINT32(TIME_STEP, timer.timeStep);
    TEST_ASSERT_EQUAL_UINT32(P2_TIME_MAX, timer.timeoutTime);
    TEST_ASSERT_TRUE(timer.timeoutStarted);
}

/******************************************************************
 * - brief:       TMR_StartTimeout arms relative to the current time
 * - description: The deadline is now + timeout; the clock itself does
 *                not move.
 * - steps:
 *     1. Timer at 85 ms, start a P2 max timeout
 *     2. Deadline is 85 + P2 max, timeout armed, clock still at 85
 ******************************************************************/
void test_pos_TMR_StartTimeout_0(void)
{
    virtTmrCtx_t timer = { .timeNow = 85u, .timeStep = TIME_STEP };

    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));

    TEST_ASSERT_EQUAL_UINT32(85u + P2_TIME_MAX, timer.timeoutTime);
    TEST_ASSERT_TRUE(timer.timeoutStarted);
    TEST_ASSERT_EQUAL_UINT32(85u, timer.timeNow);
}

/******************************************************************
 * - brief:       Restarting a timeout re-arms it from now
 * - description: Starting a running timeout again does not extend the
 *                previous deadline, it replaces it.
 * - steps:
 *     1. Start W1 max at t=0
 *     2. Wait W1 min, start W2 max
 *     3. Deadline is W1 min + W2 max
 ******************************************************************/
void test_pos_TMR_StartTimeout_1(void)
{
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP };

    /* 1. */
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, ISO9141_W1_TIME_MAX, NULL, NULL));

    /* 2. */
    TMR_DelayMs(&timer, ISO9141_W1_TIME_MIN);
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, ISO9141_W2_TIME_MAX, NULL, NULL));

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(ISO9141_W1_TIME_MIN + ISO9141_W2_TIME_MAX, timer.timeoutTime);
    TEST_ASSERT_TRUE(timer.timeoutStarted);
}

/******************************************************************
 * - brief:       A timeout expires only after its deadline
 * - description: The deadline itself still counts as in time; 1 ms
 *                later the timeout is expired.
 * - steps:
 *     1. Right after the start          -> not expired
 *     2. Exactly at the deadline        -> not expired
 *     3. 1 ms past the deadline         -> expired
 ******************************************************************/
void test_pos_TMR_IsTimeoutExpired_0(void)
{
    virtTmrCtx_t timer = { .timeNow = 100u, .timeStep = TIME_STEP };

    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, P1_TIME_MAX, NULL, NULL));

    /* 1. */
    TEST_ASSERT_FALSE(TMR_IsTimeoutExpired(&timer));

    /* 2. */
    TMR_DelayMs(&timer, P1_TIME_MAX);
    TEST_ASSERT_FALSE(TMR_IsTimeoutExpired(&timer));

    /* 3. */
    TMR_DelayMs(&timer, 1u);
    TEST_ASSERT_TRUE(TMR_IsTimeoutExpired(&timer));
}

/******************************************************************
 * - brief:       TMR_StopTimeout disarms the timeout
 * - description: Stopping clears the started flag and leaves the
 *                clock alone.
 * - steps:
 *     1. Start a P2 max timeout at t=300, then stop it
 *     2. Timeout not started, clock still at 300
 ******************************************************************/
void test_pos_TMR_StopTimeout_0(void)
{
    virtTmrCtx_t timer = { .timeNow = 300u, .timeStep = TIME_STEP };

    /* 1. */
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StopTimeout(&timer));

    /* 2. */
    TEST_ASSERT_FALSE(timer.timeoutStarted);
    TEST_ASSERT_EQUAL_UINT32(300u, timer.timeNow);
}

/******************************************************************
 * - brief:       The onOp fault hook survives Init and works
 * - description: The hook is kept across TMR_Init and can change what
 *                an operation returns: here a fresh timeout reports
 *                expired at once.
 * - steps:
 *     1. Install forceTimeoutExpired, TMR_Init, start a P2 timeout
 *     2. The hook is still installed
 *     3. The timeout reports expired at t=0
 ******************************************************************/
void test_pos_TMR_FaultHook_0(void)
{
    virtTmrCtx_t timer = { .faults = { .onOp = forceTimeoutExpired } };

    /* 1. */
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_Init(&timer));
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));

    /* 2. */
    TEST_ASSERT_EQUAL_PTR(forceTimeoutExpired, timer.faults.onOp);

    /* 3. */
    TEST_ASSERT_TRUE(TMR_IsTimeoutExpired(&timer));
    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeNow);
}

/******************************************************************
 * - brief:       A stopped timeout never reports expired
 * - description: Even after its deadline has passed, a timeout that
 *                was stopped stays not expired.
 * - steps:
 *     1. Start P1 max, stop it
 *     2. Wait past the old deadline
 *     3. Not expired
 ******************************************************************/
void test_pos_TMR_StopTimeout_1(void)
{
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP };

    /* 1. */
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, P1_TIME_MAX, NULL, NULL));
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StopTimeout(&timer));

    /* 2. */
    TMR_DelayMs(&timer, P1_TIME_MAX + 1u);

    /* 3. */
    TEST_ASSERT_FALSE(TMR_IsTimeoutExpired(&timer));
}
