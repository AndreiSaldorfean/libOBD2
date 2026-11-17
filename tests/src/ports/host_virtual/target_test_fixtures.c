/* ================================================ INCLUDES =============================================== */
#include "target_test_fixtures.h"
#include "timer_test.h"
#include "uart_test.h"
#include "fault_injection_test.h"
#include "dl_test.h"
#include "iso9141_test.h"
#include "libobd2_test.h"
#include "timer_pbt_test.h"
#include "uart_pbt_test.h"
#include "dl_pbt_test.h"
#include "iso9141_pbt_test.h"
#include "libobd2_pbt_test.h"
#include "timer_neg_test.h"
#include "uart_neg_test.h"
#include "fault_injection_neg_test.h"
#include "dl_neg_test.h"
#include "dl_fault_test.h"
#include "iso9141_neg_test.h"
#include "libobd2_neg_test.h"
#include "dl_pbt_neg_test.h"
#include "iso9141_pbt_neg_test.h"
#include "libobd2_pbt_neg_test.h"
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

/* Tests run grouped by type: positive (expected use) first, then negative (invalid input, faults,
 * timeouts); inside each, example-based before property-based, driver -> datalink -> app.
 * Traces land in test_results/ (uart_trace.h). */
int targetRunTests(void)
{
    UNITY_BEGIN();

    /*================= POSITIVE TESTS =================*/
    /* example-based */
    /* ---- DRIVER ---- */
    RUN_TEST(test_pos_ex_TMR_Init_0);
    RUN_TEST(test_pos_ex_TMR_GetTimeMs_0);
    RUN_TEST(test_pos_ex_TMR_DelayMs_0);
    RUN_TEST(test_pos_ex_TMR_StartTimeout_0);
    RUN_TEST(test_pos_ex_TMR_StartTimeout_1);
    RUN_TEST(test_pos_ex_TMR_IsTimeoutExpired_0);
    RUN_TEST(test_pos_ex_TMR_StopTimeout_0);
    RUN_TEST(test_pos_ex_TMR_FaultHook_0);
    RUN_TEST(test_pos_ex_TMR_StopTimeout_1);
    RUN_TEST(test_pos_ex_UART_WriteByte_0);
    RUN_TEST(test_pos_ex_UART_WriteByte_1);
    RUN_TEST(test_pos_ex_UART_BitBang_0);
    RUN_TEST(test_pos_ex_FAULT_Uart_0);
    RUN_TEST(test_pos_ex_FAULT_Uart_1);
    RUN_TEST(test_pos_ex_FAULT_Uart_2);
    RUN_TEST(test_pos_ex_FAULT_Uart_3);
    RUN_TEST(test_pos_ex_FAULT_Tmr_0);
    RUN_TEST(test_pos_ex_FAULT_Tmr_1);
    RUN_TEST(test_pos_ex_FAULT_Tmr_2);
    /* ---- DATALINK ---- */
    RUN_TEST(test_pos_ex_DL_ReadByteInTimeframe_0);
    RUN_TEST(test_pos_ex_DL_RecvByteBlocking_0);
    RUN_TEST(test_pos_ex_DL_SendByteBitBanged_0);
    RUN_TEST(test_pos_ex_DL_Connect_0);
    RUN_TEST(test_pos_ex_DL_SendRecv_0);
    RUN_TEST(test_pos_ex_DL_SendRequest_0);
    RUN_TEST(test_pos_ex_DL_TimeSample_0);
    RUN_TEST(test_pos_ex_DL_UartRxStatusToResponse_0);
    RUN_TEST(test_pos_ex_ISO9141_SendRecvMessage_0);
    RUN_TEST(test_pos_ex_ISO9141_5BaudInit_0);
    RUN_TEST(test_pos_ex_ISO9141_5BaudInit_1);
    RUN_TEST(test_pos_ex_ISO9141_SendRequest_0);
    /* ---- APP ---- */
    RUN_TEST(test_pos_ex_LIBOBD2_Init_0);
    RUN_TEST(test_pos_ex_LIBOBD2_RequestService_0);
    RUN_TEST(test_pos_ex_LIBOBD2_RequestService_1);
    RUN_TEST(test_pos_ex_LIBOBD2_RequestService_2);
    RUN_TEST(test_pos_ex_LIBOBD2_DispatchSid_0);
    RUN_TEST(test_pos_ex_LIBOBD2_DispatchSid_1);

    /* property-based */
    /* ---- DRIVER ---- */
    RUN_TEST(test_pos_pbt_TMR_DelayMs_0);
    RUN_TEST(test_pos_pbt_TMR_IsTimeoutExpired_0);
    RUN_TEST(test_pos_pbt_UART_RecvByte_0);
    RUN_TEST(test_pos_pbt_UART_RecvByte_1);
    RUN_TEST(test_pos_pbt_UART_RecvByte_2);
    RUN_TEST(test_pos_pbt_UART_RecvByte_3);
    /* ---- DATALINK ---- */
    RUN_TEST(test_pos_pbt_DL_ReadByteInTimeframe_0);
    RUN_TEST(test_pos_pbt_DL_ReadByteInTimeframe_1);
    RUN_TEST(test_pos_pbt_DL_ReadByteInTimeframe_2);
    RUN_TEST(test_pos_pbt_DL_RecvByteBlocking_0);
    RUN_TEST(test_pos_pbt_ISO9141_ComputeChecksum_0);
    RUN_TEST(test_pos_pbt_ISO9141_SerializeMessage_0);
    RUN_TEST(test_pos_pbt_ISO9141_RecvMessage_0);
    RUN_TEST(test_pos_pbt_ISO9141_SendRequest_0);
    RUN_TEST(test_pos_pbt_ISO9141_RecvResponse_0);
    RUN_TEST(test_pos_pbt_ISO9141_Connect_0);
    /* ---- APP ---- */
    RUN_TEST(test_pos_pbt_LIBOBD2_RequestService_0);
    RUN_TEST(test_pos_pbt_LIBOBD2_RequestService_1);
    RUN_TEST(test_pos_pbt_LIBOBD2_RequestService_2);
    RUN_TEST(test_pos_pbt_LIBOBD2_RequestService_3);

    /*================= POSITIVE TESTS =================*/

    /*================= NEGATIVE TESTS =================*/
    /* example-based */
    /* ---- DRIVER ---- */
    RUN_TEST(test_neg_ex_TMR_FaultHook_0);
    RUN_TEST(test_neg_ex_TMR_Init_0);
    RUN_TEST(test_neg_ex_TMR_IsTimeoutExpired_0);
    RUN_TEST(test_neg_ex_TMR_StartTimeout_0);
    RUN_TEST(test_neg_ex_TMR_StopTimeout_0);
    RUN_TEST(test_neg_ex_UART_FaultHook_0);
    RUN_TEST(test_neg_ex_UART_Init_0);
    RUN_TEST(test_neg_ex_UART_RecvByte_0);
    RUN_TEST(test_neg_ex_UART_RecvByte_1);
    RUN_TEST(test_neg_ex_UART_BitBang_0);
    RUN_TEST(test_neg_ex_UART_BitBang_1);
    RUN_TEST(test_neg_ex_UART_Init_1);
    RUN_TEST(test_neg_ex_FAULT_Attach_0);
    RUN_TEST(test_neg_ex_FAULT_Tmr_0);
    RUN_TEST(test_neg_ex_FAULT_Uart_0);
    /* ---- DATALINK ---- */
    RUN_TEST(test_neg_ex_DL_ReadByteInTimeframe_0);
    RUN_TEST(test_neg_ex_DL_ReadByteInTimeframe_1);
    RUN_TEST(test_neg_ex_DL_ReadByteInTimeframe_2);
    RUN_TEST(test_neg_ex_DL_ReadByteInTimeframe_3);
    RUN_TEST(test_neg_ex_DL_RecvByteBlocking_0);
    RUN_TEST(test_neg_ex_DL_RecvByteBlocking_1);
    RUN_TEST(test_neg_ex_DL_RecvByteBlocking_2);
    RUN_TEST(test_neg_ex_DL_Connect_0);
    RUN_TEST(test_neg_ex_DL_Connect_1);
    RUN_TEST(test_neg_ex_DL_Connect_2);
    RUN_TEST(test_neg_ex_DL_Api_0);
    RUN_TEST(test_neg_ex_DL_Api_1);
    RUN_TEST(test_neg_ex_DL_Api_2);
    RUN_TEST(test_neg_ex_DL_StartTimeout_0);
    RUN_TEST(test_neg_ex_DLFAULT_RecvResponse_0);
    RUN_TEST(test_neg_ex_DLFAULT_RecvResponse_1);
    RUN_TEST(test_neg_ex_DLFAULT_RecvResponse_2);
    RUN_TEST(test_neg_ex_DLFAULT_Connect_0);
    RUN_TEST(test_neg_ex_DLFAULT_Connect_1);
    RUN_TEST(test_neg_ex_DLFAULT_Connect_2);
    RUN_TEST(test_neg_ex_DLFAULT_SendRequest_0);
    RUN_TEST(test_neg_ex_DLFAULT_RecvResponse_3);
    RUN_TEST(test_neg_ex_DLFAULT_RecvResponse_4);
    RUN_TEST(test_neg_ex_DLFAULT_RecvResponse_5);
    RUN_TEST(test_neg_ex_ISO9141_RecvMessage_0);
    RUN_TEST(test_neg_ex_ISO9141_SendRequest_0);
    RUN_TEST(test_neg_ex_ISO9141_5BaudInit_0);
    RUN_TEST(test_neg_ex_ISO9141_Connect_0);
    RUN_TEST(test_neg_ex_ISO9141_Connect_1);
    RUN_TEST(test_neg_ex_ISO9141_Connect_2);
    RUN_TEST(test_neg_ex_ISO9141_Connect_3);
    RUN_TEST(test_neg_ex_ISO9141_Connect_4);
    RUN_TEST(test_neg_ex_ISO9141_Connect_5);
    RUN_TEST(test_neg_ex_ISO9141_Api_0);
    RUN_TEST(test_neg_ex_ISO9141_RecvResponse_0);
    RUN_TEST(test_neg_ex_ISO9141_RecvResponse_1);
    RUN_TEST(test_neg_ex_ISO9141_RecvResponse_2);
    RUN_TEST(test_neg_ex_ISO9141_RecvResponse_3);
    RUN_TEST(test_neg_ex_ISO9141_RecvResponse_4);
    RUN_TEST(test_neg_ex_ISO9141_RecvResponse_5);
    RUN_TEST(test_neg_ex_ISO9141_Connect_Verify_0);
    /* ---- APP ---- */
    RUN_TEST(test_neg_ex_LIBOBD2_Init_0);
    RUN_TEST(test_neg_ex_LIBOBD2_Init_1);
    RUN_TEST(test_neg_ex_LIBOBD2_Init_2);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_0);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_1);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_2);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_3);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_4);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_5);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_6);
    RUN_TEST(test_neg_ex_LIBOBD2_DecodeResponse_0);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_7);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_8);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_9);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_10);
    RUN_TEST(test_neg_ex_LIBOBD2_RequestService_11);

    /* property-based */
    /* ---- DATALINK ---- */
    RUN_TEST(test_neg_pbt_DL_ReadByteInTimeframe_0);
    RUN_TEST(test_neg_pbt_DL_ReadByteInTimeframe_1);
    RUN_TEST(test_neg_pbt_DL_ReadByteInTimeframe_2);
    RUN_TEST(test_neg_pbt_DL_ReadByteInTimeframe_3);
    RUN_TEST(test_neg_pbt_DL_RecvByteBlocking_0);
    RUN_TEST(test_neg_pbt_ISO9141_RecvResponse_0);
    RUN_TEST(test_neg_pbt_ISO9141_RecvResponse_1);
    RUN_TEST(test_neg_pbt_ISO9141_RecvResponse_2);
    RUN_TEST(test_neg_pbt_ISO9141_RecvResponse_3);
    RUN_TEST(test_neg_pbt_ISO9141_RecvResponse_4);
    RUN_TEST(test_neg_pbt_ISO9141_RecvResponse_5);
    RUN_TEST(test_neg_pbt_ISO9141_Connect_0);
    RUN_TEST(test_neg_pbt_ISO9141_SendRequest_0);
    /* ---- APP ---- */
    RUN_TEST(test_neg_pbt_LIBOBD2_RequestService_0);
    RUN_TEST(test_neg_pbt_LIBOBD2_RequestService_1);
    RUN_TEST(test_neg_pbt_LIBOBD2_RequestService_2);
    RUN_TEST(test_neg_pbt_LIBOBD2_RequestService_3);
    RUN_TEST(test_neg_pbt_LIBOBD2_RequestService_4);
    RUN_TEST(test_neg_pbt_LIBOBD2_RequestService_5);

    /*================= NEGATIVE TESTS =================*/
    return UNITY_END();
}
