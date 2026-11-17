/* ================================================ INCLUDES =============================================== */
#include "unity.h"
#include <stddef.h>
#include <unistd.h>
#include "target_test_fixtures.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
void setUp(void)
{
    targetSetUp();
}

void tearDown(void)
{
    targetTearDown();
}

/* ================================================ MODULE API ============================================= */
int main()
{
    targetPreTestsInit();

    /* Non-zero exit when a test fails, so CI can tell */
    return targetRunTests();
}
