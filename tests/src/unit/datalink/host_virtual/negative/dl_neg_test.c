/* ================================================ INCLUDES =============================================== */
#include "dl_neg_test.h"
#include "dl_fixture.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "iso9141.h"
#include "kwp2000.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define LINK_PTRS 4u /* driver pointers of a data link: uart ops, transport, timing ops, timing handle */

/* ============================================ LOCAL VARIABLES ============================================ */
/* timer_init op that succeeds only on its first call (DL_Connect's own init) */
static size_t timerInitCnt;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static obd_tmr_status_t timerInitOnce(void *pHandle)
{
    (void)pHandle;
    return (0u == timerInitCnt++) ? OBD_TMR_OK : OBD_TMR_ERR_HW;
}

/* uart init op of a transport that cannot be opened */
static bool failLine(void *handle)
{
    (void)handle;
    return false;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A byte before timeMin is read but flagged too early
 * - description: The ECU answers before the minimum wait. The byte is
 *                still handed back, but the call reports a min
 *                timeout so the protocol layer can reject it.
 * - steps:
 *     1. Byte is on the line at t=0, window is [P2 min, P2 max]
 *     2. The read returns the byte with OBD_ERR_TIMEOUT_MIN
 *     3. Elapsed time is below P2 min
 ******************************************************************/
void test_neg_DL_ReadByteInTimeframe_0(void)
{
    uint8_t byte = 0u;
    uint32_t elapsed = 0u;
    obd_status_t status = {0};

    /* 1. */
    DLFX_SetUpConnectedLine();
    DLFX_LineWriteAt(0u, 0x55u);

    /* 2. */
    status = DL_ReadByteInTimeframe(&dlFx.link, &elapsed, &byte, P2_TIME_MIN, P2_TIME_MAX);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MIN, status.timeout);
    TEST_ASSERT_EQUAL_HEX8(0x55u, byte);

    /* 3. */
    TEST_ASSERT_LESS_THAN_UINT32(P2_TIME_MIN, elapsed);
}

/******************************************************************
 * - brief:       Nothing on the line ends in a max timeout
 * - description: With no ECU answer the call gives up once timeMax
 *                has passed instead of blocking forever.
 * - steps:
 *     1. Empty line, clock at 100, window [W4 min, W4 max]
 *     2. The read returns OBD_ERR_TIMEOUT_MAX
 *     3. It stopped within one poll step after the deadline
 ******************************************************************/
void test_neg_DL_ReadByteInTimeframe_1(void)
{
    const uint32_t start = 100u;
    const uint16_t timeMax = ISO9141_W4_TIME_MAX;
    uint8_t byte = 0u;
    obd_status_t status = {0};

    /* 1. */
    DLFX_SetUpConnectedLine();
    dlFx.timer.timeNow = start;

    /* 2. */
    status = DL_ReadByteInTimeframe(&dlFx.link, NULL, &byte, ISO9141_W4_TIME_MIN, timeMax);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MAX, status.timeout);

    /* 3. */
    TEST_ASSERT_GREATER_THAN_UINT32(start + timeMax, dlFx.timer.timeNow);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(start + timeMax + DLFX_TIME_STEP, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       A UART framing error is reported in .response
 * - description: A byte that arrives with a framing error is a broken
 *                byte, not a missing one: the error goes in .response
 *                and the timeout stays OK.
 * - steps:
 *     1. The onWrite fault hook marks the byte as a framing error
 *     2. DL_ReadByteInTimeframe returns OBD_ERR_UART_FRAMING, timeout OK
 *     3. The (corrupted) byte is still handed back
 ******************************************************************/
void test_neg_DL_ReadByteInTimeframe_2(void)
{
    uint8_t byte = 0u;
    obd_status_t status = {0};

    /* 1. */
    DLFX_SetUpConnectedLine();
    dlFx.uart.faults.onWrite = DLFX_InjectFramingError;
    DLFX_LineWriteAt(30u, 0x55u);
    dlFx.uart.faults.onWrite = NULL;

    /* 2. */
    status = DL_ReadByteInTimeframe(&dlFx.link, NULL, &byte, P2_TIME_MIN, P2_TIME_MAX);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_FRAMING, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 3. */
    TEST_ASSERT_EQUAL_HEX8(0x55u, byte);
}

/******************************************************************
 * - brief:       A timeout that cannot start aborts the read
 * - description: When the window timer cannot be armed, nothing is
 *                waited for: the error goes in .response and the
 *                clock does not move.
 * - steps:
 *     1. Byte on the line at t=30, start_timeout fails
 *     2. The read returns OBD_ERR_TMR_START_FAILED, timeout OK
 *     3. The clock is still at 0 (no poll happened)
 ******************************************************************/
void test_neg_DL_ReadByteInTimeframe_3(void)
{
    uint8_t byte = 0u;
    obd_status_t status = {0};

    /* 1. */
    DLFX_SetUpConnectedLine();
    DLFX_LineWriteAt(30u, 0x55u);
    dlFx.timer.faults.onOp = DLFX_FailStartTimeout;

    /* 2. */
    status = DL_ReadByteInTimeframe(&dlFx.link, NULL, &byte, P2_TIME_MIN, P2_TIME_MAX);
    dlFx.timer.faults.onOp = NULL;
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TMR_START_FAILED, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(0u, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       RecvByteBlocking still gives up eventually
 * - description: The "blocking" read is bounded by
 *                MAX_BLOCKING_RECV_TIME.
 * - steps:
 *     1. Empty line
 *     2. DL_RecvByteBlocking returns false
 *     3. The clock is past MAX_BLOCKING_RECV_TIME
 ******************************************************************/
void test_neg_DL_RecvByteBlocking_0(void)
{
    uint8_t byte = 0u;

    /* 1. */
    DLFX_SetUpConnectedLine();

    /* 2. */
    TEST_ASSERT_FALSE(DL_RecvByteBlocking(&dlFx.link, &byte));

    /* 3. */
    TEST_ASSERT_GREATER_THAN_UINT32(MAX_BLOCKING_RECV_TIME, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       RecvByteBlocking rejects a byte with a UART error
 * - description: A byte that arrives broken ends the wait, but the
 *                call reports failure instead of handing it out as
 *                a good byte.
 * - steps:
 *     1. 0x5A arrives at t=30 with a framing error
 *     2. DL_RecvByteBlocking returns false
 *     3. It stopped at that byte, long before MAX_BLOCKING_RECV_TIME
 ******************************************************************/
void test_neg_DL_RecvByteBlocking_1(void)
{
    uint8_t byte = 0u;

    UART_TRACE_Begin("test_neg_DL_RecvByteBlocking_1.vcd");

    /* 1. */
    DLFX_SetUpConnectedLine();
    dlFx.uart.faults.onWrite = DLFX_InjectFramingError;
    DLFX_LineWriteAt(30u, 0x5Au);
    dlFx.uart.faults.onWrite = NULL;

    /* 2. */
    TEST_ASSERT_FALSE(DL_RecvByteBlocking(&dlFx.link, &byte));
    UART_TRACE_End();

    /* 3. */
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(30u + 2u * DLFX_TIME_STEP, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       RecvByteBlocking fails when its timer cannot start
 * - description: Without the safety timeout the call must not poll
 *                at all (it could never stop).
 * - steps:
 *     1. Byte on the line at t=30, start_timeout fails
 *     2. DL_RecvByteBlocking returns false
 *     3. The clock is still at 0 and the byte is still on the line
 ******************************************************************/
void test_neg_DL_RecvByteBlocking_2(void)
{
    uint8_t byte = 0u;

    /* 1. */
    DLFX_SetUpConnectedLine();
    DLFX_LineWriteAt(30u, 0x5Au);
    dlFx.timer.faults.onOp = DLFX_FailStartTimeout;

    /* 2. */
    TEST_ASSERT_FALSE(DL_RecvByteBlocking(&dlFx.link, &byte));
    dlFx.timer.faults.onOp = NULL;

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(0u, dlFx.timer.timeNow);
    dlFx.timer.timeNow = 31u;
    TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&dlFx.uart, &byte));
    TEST_ASSERT_EQUAL_HEX8(0x5Au, byte);
}

/******************************************************************
 * - brief:       No ECU on the line: DL_Connect fails
 * - description: Guards the build with KWP2000 compiled out, where
 *                ISO9141 is the only protocol tried and nothing may
 *                fall back to the KWP2000 stub.
 * - steps:
 *     1. The 5-baud address goes out, but no sync byte comes back
 *     2. DL_Connect fails with ISO9141's error (sync not received)
 *     3. Only the one 5-baud wake-up ran (no KWP2000 fast init)
 *     4. No send/recv functions are bound and no header is left
 ******************************************************************/
void test_neg_DL_Connect_0(void)
{
    obd_status_t status = {0};

    DLFX_SetUpConnectedLine();
    dlFx.link.connect       = NULL;
    dlFx.link.send_request  = NULL;
    dlFx.link.recv_response = NULL;

    UART_TRACE_Begin("test_neg_DL_Connect_0.vcd");
    status = DL_Connect(&dlFx.link);
    UART_TRACE_End();

    /* 1. + 2. */
    TEST_ASSERT_EQUAL_size_t(DLFX_BITBANG_BITS, dlFx.pulseCnt);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_5BAUD_SYNC_NOT_RECVD, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(2u, dlFx.modeCnt);
    TEST_ASSERT_EQUAL_UINT8(DIGITAL_MODE_BEGIN, dlFx.modeSeq[0]);
    TEST_ASSERT_EQUAL_UINT8(DIGITAL_MODE_END, dlFx.modeSeq[1]);

    /* 4. */
    TEST_ASSERT_NULL(dlFx.link.connect);
    TEST_ASSERT_NULL(dlFx.link.send_request);
    TEST_ASSERT_NULL(dlFx.link.recv_response);
    TEST_ASSERT_EQUAL_HEX8(0x00u, dlFx.proto.header.fmt);
}

/******************************************************************
 * - brief:       A transport that cannot open stops DL_Connect
 * - description: When the UART init fails, DL_Connect reports
 *                OBD_ERR_CONN_TRANS_INIT_FAILED and does not touch
 *                the line or the timer.
 * - steps:
 *     1. The uart init op fails
 *     2. DL_Connect returns OBD_ERR_CONN_TRANS_INIT_FAILED
 *     3. No 5-baud wake-up was sent, clock at 0, nothing bound
 ******************************************************************/
void test_neg_DL_Connect_1(void)
{
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_DL_Connect_1.vcd");
    DLFX_SetUpConnectedLine();
    dlFx.link.send_request  = NULL;
    dlFx.link.recv_response = NULL;

    /* 1. */
    dlFxUartOps.init = failLine;

    /* 2. */
    status = DL_Connect(&dlFx.link);
    dlFxUartOps.init = DLFX_KeepLine;
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_CONN_TRANS_INIT_FAILED, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(0u, dlFx.pulseCnt);
    TEST_ASSERT_EQUAL_size_t(0u, dlFx.modeCnt);
    TEST_ASSERT_EQUAL_UINT32(0u, dlFx.timer.timeNow);
    TEST_ASSERT_NULL(dlFx.link.send_request);
    TEST_ASSERT_NULL(dlFx.link.recv_response);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       Every protocol fails: DL_Connect binds nothing
 * - description: DL_Connect fails with the last protocol's error.
 *                KWP2000 clears its own context type, so the link
 *                gets a context big enough for either protocol.
 * - steps:
 *     1. ISO9141: no ECU answers -> OBD_ERR_5BAUD_SYNC_NOT_RECVD
 *     2. KWP2000 (only when LIBOBD2_SPT_KWP2000 is set, tried last):
 *        its own timer init fails -> OBD_ERR_CONN_TIMER_INIT_FAILED
 *     3. No send/recv functions are bound
 ******************************************************************/
void test_neg_DL_Connect_2(void)
{
    static union
    {
        iso9141_ctx_t iso9141;
        kwp_ctx_t kwp;
    } anyCtx;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_DL_Connect_2.vcd");
    DLFX_SetUpConnectedLine();
    dlFx.link.pProtocolCtx  = &anyCtx;
    dlFx.link.connect       = NULL;
    dlFx.link.send_request  = NULL;
    dlFx.link.recv_response = NULL;
    timerInitCnt            = 0u;
    dlFxTmrOps.timer_init   = timerInitOnce;

    /* 1. + 2. */
    status = DL_Connect(&dlFx.link);
    dlFxTmrOps.timer_init = DLFX_KeepTimer;

#ifdef LIBOBD2_SPT_KWP2000
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_CONN_TIMER_INIT_FAILED, status.response);
#else
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_5BAUD_SYNC_NOT_RECVD, status.response);
#endif /* LIBOBD2_SPT_KWP2000 */

    /* 3. */
    TEST_ASSERT_NULL(dlFx.link.send_request);
    TEST_ASSERT_NULL(dlFx.link.recv_response);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       Every DL entry point rejects a NULL data link
 * - description: NULL links and links without a protocol context are
 *                refused with OBD_ERR_NULL_PTR before touching the line.
 * - steps:
 *     1. Connect/SendRequest/RecvResponse(NULL) -> OBD_ERR_NULL_PTR
 *     2. Same with a link whose pProtocolCtx is NULL
 *     3. The clock never moved
 ******************************************************************/
void test_neg_DL_Api_0(void)
{
    obd_data_t data = { .sid = 0x01u };
    size_t len = 0u;

    /* 1. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_Connect(NULL).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_SendRequest(NULL, &data, 2u).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_RecvResponse(NULL, &data, &len).response);

    /* 2. */
    DLFX_SetUpConnectedLine();
    dlFx.link.pProtocolCtx = NULL;
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_Connect(&dlFx.link).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_SendRequest(&dlFx.link, &data, 2u).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_RecvResponse(&dlFx.link, &data, &len).response);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(0u, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       send/recv before DL_Connect fail cleanly
 * - description: Before a protocol is picked there is nothing to call:
 *                OBD_ERR_NULL_PTR instead of a crash.
 * - steps:
 *     1. Link with no send/recv bound
 *     2. DL_SendRequest and DL_RecvResponse return OBD_ERR_NULL_PTR
 *     3. The clock never moved
 ******************************************************************/
void test_neg_DL_Api_1(void)
{
    obd_data_t data = { .sid = 0x01u };
    size_t len = 0u;

    /* 1. */
    DLFX_SetUpConnectedLine();
    dlFx.link.send_request  = NULL;
    dlFx.link.recv_response = NULL;

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_SendRequest(&dlFx.link, &data, 2u).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_RecvResponse(&dlFx.link, &data, &len).response);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(0u, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       A link missing a driver pointer is rejected
 * - description: Each of the four driver pointers is removed in turn;
 *                every DL entry point refuses the link with
 *                OBD_ERR_NULL_PTR before anything goes on the line.
 * - steps:
 *     1. Remove uart ops / transport / timing ops / timing handle
 *     2. Connect, SendRequest, RecvResponse -> OBD_ERR_NULL_PTR
 *     3. The clock never moved
 ******************************************************************/
void test_neg_DL_Api_2(void)
{
    obd_data_t data = { .sid = 0x01u };
    size_t len = 0u;

    UART_TRACE_Begin("test_neg_DL_Api_2.vcd");
    for (uint8_t missing = 0u; missing < LINK_PTRS; missing++)
    {
        /* 1. */
        DLFX_SetUpConnectedLine();
        switch (missing)
        {
            case 0u: dlFx.link.pUartOps         = NULL; break;
            case 1u: dlFx.link.pTransportHandle = NULL; break;
            case 2u: dlFx.link.pTimingOps       = NULL; break;
            default: dlFx.link.pTimingHandle    = NULL; break;
        }

        /* 2. */
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_Connect(&dlFx.link).response);
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_SendRequest(&dlFx.link, &data, 2u).response);
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, DL_RecvResponse(&dlFx.link, &data, &len).response);

        /* 3. */
        TEST_ASSERT_EQUAL_UINT32(0u, dlFx.timer.timeNow);
    }
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       DL_StartTimeout reports a timer that cannot start
 * - description: The hardware error is translated into
 *                OBD_ERR_TMR_START_FAILED.
 * - steps:
 *     1. start_timeout fails through the onOp hook
 *     2. DL_StartTimeout returns OBD_ERR_TMR_START_FAILED
 ******************************************************************/
void test_neg_DL_StartTimeout_0(void)
{
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_DL_StartTimeout_0.vcd");

    /* 1. */
    DLFX_SetUpConnectedLine();
    dlFx.timer.faults.onOp = DLFX_FailStartTimeout;

    /* 2. */
    status = DL_StartTimeout(&dlFx.link, P2_TIME_MAX);
    dlFx.timer.faults.onOp = NULL;
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TMR_START_FAILED, status.response);
    UART_TRACE_End();
}
