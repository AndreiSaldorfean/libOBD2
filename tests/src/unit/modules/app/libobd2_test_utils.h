#ifndef LIBOBD2_TEST_UTILS_H
#define LIBOBD2_TEST_UTILS_H
/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "l2_iso9141.h"
#include "l2_kwp2000.h"
#include "libobd2_timer_port.h"
#include "libobd2.h"
#include "statusRetCodes.h"
#include "libobd2_uart_port.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "unity.h"
#include <stdio.h>

/* ================================================= MACROS ================================================ */
#define TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual) \
    TEST_ASSERT_EQUAL_HEX16(expected.response, actual.response); \
    TEST_ASSERT_EQUAL_HEX16(expected.timeout, actual.timeout)

static inline void TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(obd_status_t expected, obd_status_t actual, const char* msg)
{
    int failed = 0;
    if (expected.response != actual.response) {
        UNITY_OUTPUT_CHAR('\r'); UNITY_OUTPUT_CHAR('\n');
        UnityPrint(" resp: expected 0x"); UnityPrintNumberHex(expected.response, 4);
        UnityPrint(" actual 0x");             UnityPrintNumberHex(actual.response,   4);
        failed = 1;
    }
    if (expected.timeout != actual.timeout) {
        UNITY_OUTPUT_CHAR('\r'); UNITY_OUTPUT_CHAR('\n');
        UnityPrint(" timeout: expected 0x");  UnityPrintNumberHex(expected.timeout, 4);
        UnityPrint(" actual 0x");             UnityPrintNumberHex(actual.timeout,   4);
        failed = 1;
    }
    if (failed) {
        UNITY_OUTPUT_CHAR('\r'); UNITY_OUTPUT_CHAR('\n');
        TEST_FAIL_MESSAGE(msg);
    }
}

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
extern const header_t header_00_ecu;
extern const header_t header_00;
extern const obd_data_t request_00;
extern const obd_data_t data_00;
extern const message_t msg_00;
extern const message_t msg_00_ecu;
extern timerCtx_t tmrCtxTx;
extern timerCtx_t tmrCtxRx;
extern obd_timing_ops_t timerOpsRx;
extern obd_timing_ops_t timerOpsTx;
extern uart_ctx_t uartCtxTx;
// extern timerCtx_t tmrCtxTx;
extern uart_ctx_t uartCtxRx;
// extern timerCtx_t tmrCtxRx;
extern obd_uart_ops_t transportOps;
extern l2_iso9141_ctx_t iso9141CtxTx;
extern l2_iso9141_ctx_t iso9141CtxRx;
extern dataLink_if_t dataLink_00;
extern dataLink_if_t dataLink_tx;
extern dataLink_if_t dataLink_rx;
extern obd_ctx_t ctxTx;
extern obd_ctx_t ctxRx;
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_TEST_UTILS_H */
