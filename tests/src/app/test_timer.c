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

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_TIMER_0(void)
{
    timerCtx_t tmrCtx =
    {
        .timeout_active = false,
        .timeout_expired = false,
        .timeout_target_ms = 0,
    };

    KWP_TMR_Init(&tmrCtx);

    KWP_TMR_DelayMs(&tmrCtx, 10);
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

    KWP_TMR_Init(&tmrCtx);
}
