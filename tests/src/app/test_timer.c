/* ================================================ INCLUDES =============================================== */
#include "test_timer.h"
#include "libobd2.h"
#include "srv_status.h"
#include "transport_if.h"
#include "iso15031_5.h"
#include "tusb.h"
#include "unity.h"
#include <stdio.h>
#include <time.h>
#include "kwp_timer.h"
#include "utils.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_TIMER_0(void)
{
    bool timeoutExpired = true;
    timerCtx_t tmrCtx =
    {
        .timeout_active = false,
        .timeout_expired = false,
        .timeout_target_ms = 0,
    };

    KWP_TMR_Init(&tmrCtx);

    KWP_TMR_DelayMs(&tmrCtx, 1000);

    KWP_TMR_StartTimeout(&tmrCtx, 1000, NULL, NULL);

    for(int i=0;i<10000;i++)
    {
        __asm__("nop");
    }

    timeoutExpired = KWP_TMR_IsTimeoutExpired(&tmrCtx);

    KWP_TMR_StopTimeout(&tmrCtx);

    KWP_TMR_StartTimeout(&tmrCtx, 10, NULL, NULL);

    for(int i=0;i<1000000;i++)
    {
        __asm__("nop");
    }

    timeoutExpired = KWP_TMR_IsTimeoutExpired(&tmrCtx);

    KWP_TMR_StopTimeout(&tmrCtx);
}

void test_TIMER_1(void)
{
    timerCtx_t tmrCtx =
    {
        .timeout_active = false,
        .timeout_expired = false,
        .timeout_callback = NULL,
        .timeout_user_data = NULL,
        .timeout_target_ms = 0,
    };
    uint32_t time1 = 0;
    uint32_t time2 = 0;
    uint32_t elapsed = 0;

    KWP_TMR_Init(&tmrCtx);

    time1 = KWP_TMR_GetTimeMs(&tmrCtx);

    KWP_TMR_DelayMs(&tmrCtx, 10);

    time2 = KWP_TMR_GetTimeMs(&tmrCtx);

    elapsed = time2 - time1;
    TEST_ASSERT_EQUAL(elapsed, 10);
}
