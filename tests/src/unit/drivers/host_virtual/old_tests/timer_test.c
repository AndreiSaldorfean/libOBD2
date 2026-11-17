/* ================================================ INCLUDES =============================================== */
#include "timer_test.h"
#include "libobd2_timer_port.h"
#include "timing_if.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include "driver_test_vectors_001.h"
/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
extern bool TMR_Init(void *pHandle);
extern uint32_t TMR_GetTimeMs(void *pHandle);
extern void TMR_DelayMs(void *pHandle, uint32_t delay_ms);
extern bool TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData);
extern bool TMR_StopTimeout(void *pHandle);
extern bool TMR_IsTimeoutExpired(void *pHandle);

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_TMR_Init_0(void)
{
    TEST_ASSERT_EQUAL_UINT32(1u, TMR_Init(&tmr_ctx_tester));
}
