/* ================================================ INCLUDES =============================================== */
#include "timer_test.h"
#include "datalink.h"
#include "l2_iso9141.h"
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
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * Init puts the virtual clock back to a known state:
 * time, step and timeout are all cleared.
 ******************************************************************/
void test_TMR_Init_000(void)
{
    virtTmrCtx_t timer =
    {
        .timeNow        = 1234u,
        .timeStep       = TIME_STEP,
        .timeoutTime    = 5678u,
        .timeoutStarted = true,
    };

    TEST_ASSERT_TRUE(LIBOBD2_TMR_Init(&timer));

    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeNow);
    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeStep);
    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeoutTime);
    TEST_ASSERT_FALSE(timer.timeoutStarted);
}

/******************************************************************
 * GetTimeMs reports the virtual clock without moving it.
 ******************************************************************/
void test_TMR_GetTimeMs_000(void)
{
    virtTmrCtx_t timer = { .timeNow = 2360u, .timeStep = TIME_STEP };

    TEST_ASSERT_EQUAL_UINT32(2360u, LIBOBD2_TMR_GetTimeMs(&timer));
    TEST_ASSERT_EQUAL_UINT32(2360u, LIBOBD2_TMR_GetTimeMs(&timer));
    TEST_ASSERT_EQUAL_UINT32(2360u, timer.timeNow);
}

/******************************************************************
 * DelayMs moves the clock forward by exactly the delay, one ISO9141
 * request worth of delays: P3 idle, then P4 after each of 6 bytes.
 * A 0 ms delay is a no-op, and the running timeout is left alone.
 ******************************************************************/
void test_TMR_DelayMs_000(void)
{
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP };

    TEST_ASSERT_TRUE(LIBOBD2_TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));

    LIBOBD2_TMR_DelayMs(&timer, P3_TIME_MIN);
    TEST_ASSERT_EQUAL_UINT32(P3_TIME_MIN, LIBOBD2_TMR_GetTimeMs(&timer));

    for (uint8_t idx = 0; idx < 6u; idx++)
    {
        LIBOBD2_TMR_DelayMs(&timer, P4_TIME_MIN);
    }
    TEST_ASSERT_EQUAL_UINT32(P3_TIME_MIN + 6u * P4_TIME_MIN, LIBOBD2_TMR_GetTimeMs(&timer));

    LIBOBD2_TMR_DelayMs(&timer, 0u);
    TEST_ASSERT_EQUAL_UINT32(P3_TIME_MIN + 6u * P4_TIME_MIN, LIBOBD2_TMR_GetTimeMs(&timer));

    TEST_ASSERT_EQUAL_UINT32(TIME_STEP, timer.timeStep);
    TEST_ASSERT_EQUAL_UINT32(P2_TIME_MAX, timer.timeoutTime);
    TEST_ASSERT_TRUE(timer.timeoutStarted);
}

/******************************************************************
 * StartTimeout arms the timeout relative to the current time.
 ******************************************************************/
void test_TMR_StartTimeout_000(void)
{
    virtTmrCtx_t timer = { .timeNow = 85u, .timeStep = TIME_STEP };

    TEST_ASSERT_TRUE(LIBOBD2_TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));

    TEST_ASSERT_EQUAL_UINT32(85u + P2_TIME_MAX, timer.timeoutTime);
    TEST_ASSERT_TRUE(timer.timeoutStarted);
    TEST_ASSERT_EQUAL_UINT32(85u, timer.timeNow);
}

/******************************************************************
 * Restarting a running timeout re-arms it from the new current
 * time, it does not extend the previous deadline.
 ******************************************************************/
void test_TMR_StartTimeout_001(void)
{
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP };

    TEST_ASSERT_TRUE(LIBOBD2_TMR_StartTimeout(&timer, ISO9141_W1_TIME_MAX, NULL, NULL));
    LIBOBD2_TMR_DelayMs(&timer, ISO9141_W1_TIME_MIN);
    TEST_ASSERT_TRUE(LIBOBD2_TMR_StartTimeout(&timer, ISO9141_W2_TIME_MAX, NULL, NULL));

    TEST_ASSERT_EQUAL_UINT32(ISO9141_W1_TIME_MIN + ISO9141_W2_TIME_MAX, timer.timeoutTime);
    TEST_ASSERT_TRUE(timer.timeoutStarted);
}

/******************************************************************
 * The timeout expires only once the deadline has been passed:
 *   1. right after the start          -> not expired
 *   2. exactly at the deadline        -> not expired
 *   3. 1 ms past the deadline         -> expired
 ******************************************************************/
void test_TMR_IsTimeoutExpired_000(void)
{
    virtTmrCtx_t timer = { .timeNow = 100u, .timeStep = TIME_STEP };

    TEST_ASSERT_TRUE(LIBOBD2_TMR_StartTimeout(&timer, P1_TIME_MAX, NULL, NULL));

    /* 1. */
    TEST_ASSERT_FALSE(LIBOBD2_TMR_IsTimeoutExpired(&timer));

    /* 2. */
    LIBOBD2_TMR_DelayMs(&timer, P1_TIME_MAX);
    TEST_ASSERT_FALSE(LIBOBD2_TMR_IsTimeoutExpired(&timer));

    /* 3. */
    LIBOBD2_TMR_DelayMs(&timer, 1u);
    TEST_ASSERT_TRUE(LIBOBD2_TMR_IsTimeoutExpired(&timer));
}

/******************************************************************
 * StopTimeout disarms the timeout and leaves the clock alone.
 ******************************************************************/
void test_TMR_StopTimeout_000(void)
{
    virtTmrCtx_t timer = { .timeNow = 300u, .timeStep = TIME_STEP };

    TEST_ASSERT_TRUE(LIBOBD2_TMR_StartTimeout(&timer, P2_TIME_MAX, NULL, NULL));
    TEST_ASSERT_TRUE(LIBOBD2_TMR_StopTimeout(&timer));

    TEST_ASSERT_FALSE(timer.timeoutStarted);
    TEST_ASSERT_EQUAL_UINT32(300u, timer.timeNow);
}
