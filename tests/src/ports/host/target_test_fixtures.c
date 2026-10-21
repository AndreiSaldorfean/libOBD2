/* ================================================ INCLUDES =============================================== */
#include "target_test_fixtures.h"
#include "timer_test.h"
#include "iso9141_test.h"
#include "uart_test.h"
#include "uart_pbt_test.h"
#include "iso9141_pbt_test.h"
#include "dl_pbt_test.h"
#include "dl_test.h"
#include "libobd2_pbt_test.h"
#include "libobd2_test.h"
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

    /*================= SANITY TESTS =================*/
    /* ---- DRIVER TESTS ---- */
    RUN_TEST(test_TMR_Init_000);
    RUN_TEST(test_TMR_GetTimeMs_000);
    RUN_TEST(test_TMR_DelayMs_000);
    RUN_TEST(test_TMR_StartTimeout_000);
    RUN_TEST(test_TMR_StartTimeout_001);
    RUN_TEST(test_TMR_IsTimeoutExpired_000);
    RUN_TEST(test_TMR_StopTimeout_000);

    RUN_TEST(test_UART_WriteByte_0);
    RUN_TEST(test_UART_WriteByte_1);
    /* ---- DRIVER TESTS ---- */

    /* ---- DATALINK TESTS ---- */
    /* COMMON */
    RUN_TEST(test_DL_ReadByteInTimeframe_000);
    RUN_TEST(test_DL_ReadByteInTimeframe_TooEarly);
    RUN_TEST(test_DL_ReadByteInTimeframe_NoByte);
    RUN_TEST(test_DL_RecvByteBlocking_000);
    RUN_TEST(test_DL_RecvByteBlocking_NoByte);
    RUN_TEST(test_DL_SendByteBitBanged_000);
    RUN_TEST(test_DL_Connect_000);
    RUN_TEST(test_DL_SendRecv_000);

    /* ISO9141 */
    RUN_TEST(test_ISO9141_SendRecvMessage_000);
    RUN_TEST(test_ISO9141_5BaudInit_000);
    /* ---- DATALINK TESTS ---- */


    /* ---- APP TESTS ---- */
    RUN_TEST(test_LIBOBD2_Init_NullCtx);
    RUN_TEST(test_LIBOBD2_Init_NullDataLink);
    RUN_TEST(test_LIBOBD2_Init_000);
    RUN_TEST(test_LIBOBD2_RequestService_NullResponse);
    RUN_TEST(test_LIBOBD2_RequestService_NullResponseLen);
    RUN_TEST(test_LIBOBD2_RequestService_InvalidSid);
    RUN_TEST(test_LIBOBD2_RequestService_VIN_000);
    RUN_TEST(test_LIBOBD2_RequestService_CALID_000);
    RUN_TEST(test_LIBOBD2_RequestService_CoolantTemp_000);
    /* ---- APP TESTS ---- */
    /*================= SANITY TESTS =================*/

    /*================= STRESS TESTS =================*/
    // RUN_TEST(test_UART_PBT_WriteReadRoundtrip);
    // RUN_TEST(test_UART_PBT_ByteNotReadyBeforeTimestamp);
    // RUN_TEST(test_UART_PBT_IncTimeOnByteRead);
    //
    // RUN_TEST(test_DL_PBT_ReadByteInTimeframe_ReturnsByteWithinWindow);
    // RUN_TEST(test_DL_PBT_ReadByteInTimeframe_TimesOutWhenByteNeverArrives);
    // RUN_TEST(test_DL_PBT_RecvByteBlocking_ReturnsPushedByte);
    //
    // RUN_TEST(test_ISO9141_PBT_ComputeCheckSum);
    // RUN_TEST(test_ISO9141_PBT_PrepareMessageSerializesCorrectly);
    // RUN_TEST(test_ISO9141_PBT_SendThenRecvRoundtrip);
    //
    // RUN_TEST(test_LIBOBD2_PBT_VinRoundtripAcrossMultiFrameResponse);
    // RUN_TEST(test_LIBOBD2_PBT_SendFailurePropagatesStatusUntouched);
    /*================= STRESS TESTS =================*/

    UNITY_END();

}
