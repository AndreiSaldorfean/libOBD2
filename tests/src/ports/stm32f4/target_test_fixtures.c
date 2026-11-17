/* ================================================ INCLUDES =============================================== */
#include "target_test_fixtures.h"
#include "init.h"
#include "unity.h"

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void test_target_build(void)
{
    TEST_ASSERT_TRUE(1);
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
