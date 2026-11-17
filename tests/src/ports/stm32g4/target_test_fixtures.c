/* ================================================ INCLUDES =============================================== */
#include "target_test_fixtures.h"
#include "init.h"
#include "unity.h"
#include <stdint.h>
/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void test_target_build(void)
{
    TEST_ASSERT_TRUE(1);
}

/* The production STM32G4 example supplies _write through SEGGER RTT. The
 * test image does not link RTT, so provide the silent sink Unity needs. */
int _write(int file, char *ptr, uint32_t len)
{
    (void)file;
    (void)ptr;
    return (int)len;
}

/* ================================================ MODULE API ============================================= */
void targetSetUp(void)
{
}

void targetTearDown(void)
{
}

void targetPreTestsInit(void)
{
    sysInit();
}

int targetRunTests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_target_build);
    return UNITY_END();
}
