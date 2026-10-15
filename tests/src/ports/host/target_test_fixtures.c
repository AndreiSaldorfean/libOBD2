/* ================================================ INCLUDES =============================================== */
#include "target_test_fixtures.h"
// #include "timer_test.h"
// #include "unit/drivers/host/timer_test.h"
// #include "unit/drivers/host/uart_test.h"
#include "uart_pbt_test.h"
#include "iso9141_pbt_test.h"
#include "dl_pbt_test.h"
#include "libobd2_pbt_test.h"
#include "unity_internals.h"
/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void targetSetUp()
{
}

void targetTearDown()
{
}

void targetPreTestsInit()
{

}

void targetRunTests()
{
    UNITY_BEGIN();

    // RUN_TEST(test_TMR_Init_0);
    // RUN_TEST(test_UART_WriteByte_0);
    RUN_TEST(test_UART_PBT_WriteReadRoundtrip);
    RUN_TEST(test_UART_PBT_ByteNotReadyBeforeTimestamp);
    RUN_TEST(test_UART_PBT_IncTimeOnByteRead);

    RUN_TEST(test_DL_PBT_ReadByteInTimeframe_ReturnsByteWithinWindow);
    RUN_TEST(test_DL_PBT_ReadByteInTimeframe_TimesOutWhenByteNeverArrives);
    RUN_TEST(test_DL_PBT_RecvByteBlocking_ReturnsPushedByte);

    RUN_TEST(test_ISO9141_PBT_ComputeCheckSum);
    RUN_TEST(test_ISO9141_PBT_PrepareMessageSerializesCorrectly);
    RUN_TEST(test_ISO9141_PBT_SendThenRecvRoundtrip);

    RUN_TEST(test_LIBOBD2_PBT_VinRoundtripAcrossMultiFrameResponse);
    RUN_TEST(test_LIBOBD2_PBT_SendFailurePropagatesStatusUntouched);

    UNITY_END();

}
