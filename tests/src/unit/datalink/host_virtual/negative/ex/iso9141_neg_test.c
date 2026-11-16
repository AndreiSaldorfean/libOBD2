/* ================================================ INCLUDES =============================================== */
#include "iso9141_neg_test.h"
#include "dl_fixture.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "datalink_test_vectors_000.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_trace.h"
#include "unity.h"

/* ================================================= MACROS ================================================ */
#define ECU_SYNC_IDX      0u
#define ECU_KB1_IDX       1u
#define ECU_KB2_IDX       2u
#define ECU_INV_ADDR_IDX  3u
#define NO_CHECKSUM_DATA  13u /* data bytes of a frame that never closes with a checksum */

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern obd_status_t ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t *len);
extern obd_status_t ISO9141_5BaudInit(dataLink_if_t *self, uint8_t *protocol);
extern obd_status_t ISO9141_Connect_Verify(dataLink_if_t *self);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* Runs ISO9141_Connect against the given ECU answer and checks that it fails with 'expResponse',
 * leaving the protocol untouched and no header in the context */
static void connectExpectFailure(const char *trace, const dlFxEcuInit_t *ecu, uint16_t expResponse)
{
    uint8_t protocol = 0xFFu;
    obd_status_t status = {0};

    DLFX_SetUpLine();
    DLFX_QueueEcu5BaudInit(ecu);

    UART_TRACE_Begin(trace);
    status = ISO9141_Connect(&dlFx.link, &protocol);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(expResponse, status.response);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, protocol);
    TEST_ASSERT_EQUAL_HEX8(0x00u, dlFx.proto.header.fmt);
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       The ECU sends only the format byte of a response
 * - description: The format byte arrives after P2 min, so the header
 *                read starts, but the target byte never comes: the
 *                error must be OBD_ERR_COMM_DATA_BYTE_NOT_RECVD with
 *                the P1 timeout, not a bare timeout with response OK.
 * - steps:
 *     1. Only 0x48 is on the line, after P2 min
 *     2. ISO9141_RecvMessage waits P1 max for the target byte
 *     3. Returns OBD_ERR_COMM_DATA_BYTE_NOT_RECVD + OBD_ERR_TIMEOUT_MAX,
 *        the format byte is kept
 ******************************************************************/
void test_neg_ex_ISO9141_RecvMessage_0(void)
{
    const uint8_t fmt = 0x48u;
    message_t recvdMsg = {0};
    size_t recvdLen = 0;
    obd_status_t status = {0};

    DLFX_SetUpLine();

    /* 1. */
    DLFX_QueueEcuFrame(&fmt, 1u);

    /* 2. */
    status = ISO9141_RecvMessage(&dlFx.link, &recvdMsg, &recvdLen);

    /* 3. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_DATA_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MAX, status.timeout);
    TEST_ASSERT_EQUAL_HEX8(0x48u, recvdMsg.header.fmt);
}

/******************************************************************
 * - brief:       send_request rejects data lengths outside 1..7
 * - description: 0 would underflow in SerializeMessage, 8 would
 *                overrun the frame buffer. A rejected request puts
 *                nothing on the line: no P3 wait, no bytes.
 * - steps:
 *     1. dataLen 0 -> OBD_ERR_INVALID_PARAM
 *     2. dataLen 8 -> OBD_ERR_INVALID_PARAM
 *     3. The clock never moved
 ******************************************************************/
void test_neg_ex_ISO9141_SendRequest_0(void)
{
    obd_data_t data = { .sid = 0x01u };

    tmr_ctx_tester.timeNow = 0u;

    /* 1. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, ISO9141_SendRequest(&dl_tester, &data, 0u).response);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, ISO9141_SendRequest(&dl_tester, &data, 8u).response);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(0u, tmr_ctx_tester.timeNow);
}

/******************************************************************
 * - brief:       Unknown keyword bytes fail the 5-baud init
 * - description: The ECU completes the handshake but sends keyword
 *                bytes 12 34, which match neither ISO9141-2 (08 08)
 *                nor KWP2000 (94 94).
 * - steps:
 *     1. ECU answers 55 12 34 CC
 *     2. ISO9141_5BaudInit returns OBD_ERR_5BAUD_WRONG_KEYBYTES
 *     3. Protocol untouched, no header stored
 ******************************************************************/
void test_neg_ex_ISO9141_5BaudInit_0(void)
{
    dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;
    uint8_t protocol = 0xFFu;
    obd_status_t status = {0};

    DLFX_SetUpLine();
    dlFx.proto = (iso9141_ctx_t){0};

    /* 1. */
    ecu.kb1 = 0x12u;
    ecu.kb2 = 0x34u;
    DLFX_QueueEcu5BaudInit(&ecu);

    /* 2. */
    UART_TRACE_Begin("test_neg_ex_ISO9141_5BaudInit_0.vcd");
    status = ISO9141_5BaudInit(&dlFx.link, &protocol);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_5BAUD_WRONG_KEYBYTES, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_HEX8(0xFFu, protocol);
    TEST_ASSERT_EQUAL_HEX8(0x00u, dlFx.proto.header.fmt);
}

/******************************************************************
 * - brief:       A wrong sync byte fails the 5-baud init
 * - description: The ECU answers the wake-up address in time, but
 *                with 5A instead of the 55 sync pattern.
 * - steps:
 *     1. ECU answers 5A 08 08 CC
 *     2. ISO9141_Connect returns OBD_ERR_5BAUD_WRONG_SYNC_BYTE
 *     3. Protocol untouched, no header stored
 ******************************************************************/
void test_neg_ex_ISO9141_Connect_0(void)
{
    dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;

    /* 1. */
    ecu.sync = 0x5Au;

    /* 2. + 3. */
    connectExpectFailure("test_neg_ex_ISO9141_Connect_0.vcd", &ecu, OBD_ERR_5BAUD_WRONG_SYNC_BYTE);
}

/******************************************************************
 * - brief:       A sync byte after W1 max fails the 5-baud init
 * - description: The ECU wakes up too slowly: the sync byte arrives
 *                300 ms after its earliest time, past W1 max.
 * - steps:
 *     1. ECU answers 55 08 08 CC, everything 300 ms late
 *     2. ISO9141_Connect returns OBD_ERR_5BAUD_SYNC_NOT_RECVD
 *     3. Protocol untouched, no header stored
 ******************************************************************/
void test_neg_ex_ISO9141_Connect_1(void)
{
    dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;

    /* 1. */
    ecu.lateIdx = ECU_SYNC_IDX;
    ecu.lateMs  = ISO9141_W1_TIME_MAX;

    /* 2. + 3. */
    connectExpectFailure("test_neg_ex_ISO9141_Connect_1.vcd", &ecu, OBD_ERR_5BAUD_SYNC_NOT_RECVD);
}

/******************************************************************
 * - brief:       KB1 after W2 max fails the 5-baud init
 * - description: The sync byte is fine, but the first keyword byte
 *                comes 50 ms late, past the W2 window.
 * - steps:
 *     1. ECU answers 55 on time, then 08 08 CC 50 ms late
 *     2. ISO9141_Connect returns OBD_ERR_5BAUD_KB1_NOT_RECVD
 *     3. Protocol untouched, no header stored
 ******************************************************************/
void test_neg_ex_ISO9141_Connect_2(void)
{
    dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;

    /* 1. */
    ecu.lateIdx = ECU_KB1_IDX;
    ecu.lateMs  = 50u;

    /* 2. + 3. */
    connectExpectFailure("test_neg_ex_ISO9141_Connect_2.vcd", &ecu, OBD_ERR_5BAUD_KB1_NOT_RECVD);
}

/******************************************************************
 * - brief:       KB2 after W3 max fails the 5-baud init
 * - description: Sync and KB1 are fine, the second keyword byte comes
 *                50 ms late, past the W3 window.
 * - steps:
 *     1. ECU answers 55 08 on time, then 08 CC 50 ms late
 *     2. ISO9141_Connect returns OBD_ERR_5BAUD_KB2_NOT_RECVD
 *     3. Protocol untouched, no header stored
 ******************************************************************/
void test_neg_ex_ISO9141_Connect_3(void)
{
    dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;

    /* 1. */
    ecu.lateIdx = ECU_KB2_IDX;
    ecu.lateMs  = 50u;

    /* 2. + 3. */
    connectExpectFailure("test_neg_ex_ISO9141_Connect_3.vcd", &ecu, OBD_ERR_5BAUD_KB2_NOT_RECVD);
}

/******************************************************************
 * - brief:       An inverted address after W4 max fails the init
 * - description: The ECU never confirms the tester's ~KB2 in time:
 *                the inverted address comes 100 ms late.
 * - steps:
 *     1. ECU answers 55 08 08 on time, CC 100 ms late
 *     2. ISO9141_Connect returns OBD_ERR_5BAUD_INV_ADDR_NOT_RECVD
 *     3. Protocol untouched, no header stored
 ******************************************************************/
void test_neg_ex_ISO9141_Connect_4(void)
{
    dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;

    /* 1. */
    ecu.lateIdx = ECU_INV_ADDR_IDX;
    ecu.lateMs  = 100u;

    /* 2. + 3. */
    connectExpectFailure("test_neg_ex_ISO9141_Connect_4.vcd", &ecu, OBD_ERR_5BAUD_INV_ADDR_NOT_RECVD);
}

/******************************************************************
 * - brief:       A wrong inverted address fails the 5-baud init
 * - description: The ECU echoes the address (33) instead of its
 *                inverse (CC) at the end of the handshake.
 * - steps:
 *     1. ECU answers 55 08 08 33
 *     2. ISO9141_Connect returns OBD_ERR_5BAUD_WRONG_INV_ADDR
 *     3. Protocol untouched, no header stored
 ******************************************************************/
void test_neg_ex_ISO9141_Connect_5(void)
{
    dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;

    /* 1. */
    ecu.invAddr = DLFX_TARGET_ADDR;

    /* 2. + 3. */
    connectExpectFailure("test_neg_ex_ISO9141_Connect_5.vcd", &ecu, OBD_ERR_5BAUD_WRONG_INV_ADDR);
}

/******************************************************************
 * - brief:       Every public ISO9141 entry point rejects NULL args
 * - description: OBD_ERR_NULL_PTR is returned before anything touches
 *                the line.
 * - steps:
 *     1. Connect(protocol NULL), SendRequest(data NULL),
 *        RecvResponse(resp NULL), RecvResponse(len NULL)
 *     2. All return OBD_ERR_NULL_PTR
 *     3. The clock never moved
 ******************************************************************/
void test_neg_ex_ISO9141_Api_0(void)
{
    obd_data_t data = {0};
    size_t len = 0u;

    UART_TRACE_Begin("test_neg_ex_ISO9141_Api_0.vcd");
    tmr_ctx_tester.timeNow = 0u;

    /* 1. + 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, ISO9141_Connect(&dl_tester, NULL).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, ISO9141_SendRequest(&dl_tester, NULL, 2u).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, ISO9141_RecvResponse(&dl_tester, NULL, &len).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, ISO9141_RecvResponse(&dl_tester, &data, NULL).response);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(0u, tmr_ctx_tester.timeNow);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A negative ECU response is reported, not returned
 * - description: The ECU rejects the request with sid 7F; the caller
 *                gets OBD_ERR_COMM_ECU_RESPONSE_7F.
 *                ECU: 48 6B 12 7F 01 12 57 (rejected sid 01, NRC 12)
 * - steps:
 *     1. Queue the negative response
 *     2. ISO9141_RecvResponse returns OBD_ERR_COMM_ECU_RESPONSE_7F,
 *        timeout OK
 ******************************************************************/
void test_neg_ex_ISO9141_RecvResponse_0(void)
{
    const uint8_t frame[] = { 0x48u, 0x6Bu, 0x12u, 0x7Fu, 0x01u, 0x12u, 0x57u };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_ISO9141_RecvResponse_0.vcd");
    DLFX_SetUpLine();

    /* 1. */
    DLFX_QueueEcuFrame(frame, sizeof(frame));

    /* 2. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_ECU_RESPONSE_7F, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A frame without a sid is rejected
 * - description: The checksum follows the header directly, so the
 *                frame carries no data at all.
 *                ECU: 48 6B 12 C5
 * - steps:
 *     1. Queue the empty frame
 *     2. ISO9141_RecvResponse returns OBD_ERR_COMM_RECV_MSG_FAILED,
 *        timeout OK
 ******************************************************************/
void test_neg_ex_ISO9141_RecvResponse_1(void)
{
    const uint8_t frame[] = { 0x48u, 0x6Bu, 0x12u, 0xC5u };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_ISO9141_RecvResponse_1.vcd");
    DLFX_SetUpLine();

    /* 1. */
    DLFX_QueueEcuFrame(frame, sizeof(frame));

    /* 2. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_RECV_MSG_FAILED, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A frame that never closes is rejected safely
 * - description: The data field fills up without a byte matching the
 *                running checksum; the read stops instead of
 *                overrunning message_t.
 *                ECU: 48 6B 12, then 13 x 01 (the sum never comes back to 01)
 * - steps:
 *     1. Queue the frame without checksum
 *     2. ISO9141_RecvResponse returns OBD_ERR_COMM_RECV_MSG_FAILED,
 *        timeout OK
 ******************************************************************/
void test_neg_ex_ISO9141_RecvResponse_2(void)
{
    uint8_t frame[3u + NO_CHECKSUM_DATA] = { 0x48u, 0x6Bu, 0x12u };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_ISO9141_RecvResponse_2.vcd");
    for (size_t idx = 3u; idx < sizeof(frame); idx++)
    {
        frame[idx] = 0x01u;
    }
    DLFX_SetUpLine();

    /* 1. */
    DLFX_QueueEcuFrame(frame, sizeof(frame));

    /* 2. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_RECV_MSG_FAILED, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       No ECU answer within P2 max
 * - description: The request went out, but the ECU stays silent: the
 *                response is missing with the P2 max timeout.
 * - steps:
 *     1. Empty line, P2 started at t=0
 *     2. ISO9141_RecvResponse returns OBD_ERR_COMM_FMT_BYTE_NOT_RECVD
 *        with OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU
 *     3. It gave up within one poll step after P2 max
 ******************************************************************/
void test_neg_ex_ISO9141_RecvResponse_3(void)
{
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_ISO9141_RecvResponse_3.vcd");

    /* 1. */
    DLFX_SetUpLine();
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, DL_StartTimeout(&dlFx.link, P2_TIME_MAX).response);

    /* 2. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU, status.timeout);

    /* 3. */
    TEST_ASSERT_GREATER_THAN_UINT32(P2_TIME_MAX, dlFx.timer.timeNow);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(P2_TIME_MAX + DLFX_TIME_STEP, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       An ECU answer before P2 min is rejected
 * - description: The ECU starts talking 5 ms after the request, while
 *                the tester must still be waiting for P2 min.
 * - steps:
 *     1. ECU frame 48 6B 12 41 05 7B 86 from t=5, P2 started at t=0
 *     2. ISO9141_RecvResponse returns OBD_ERR_COMM_FMT_BYTE_NOT_RECVD
 *        with OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU
 ******************************************************************/
void test_neg_ex_ISO9141_RecvResponse_4(void)
{
    const uint8_t frame[] = { 0x48u, 0x6Bu, 0x12u, 0x41u, 0x05u, 0x7Bu, 0x86u };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_ISO9141_RecvResponse_4.vcd");
    DLFX_SetUpLine();

    /* 1. */
    for (size_t idx = 0u; idx < sizeof(frame); idx++)
    {
        DLFX_LineWriteAt(5u + (uint32_t)idx * DLFX_ECU_BYTE_GAP, frame[idx]);
    }
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, DL_StartTimeout(&dlFx.link, P2_TIME_MAX).response);

    /* 2. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU, status.timeout);
}

/******************************************************************
 * - brief:       The ECU stalls in the middle of a frame
 * - description: After 48 6B the ECU pauses longer than P1 max before
 *                the rest of the frame; the tester gives up on the
 *                frame instead of gluing the late bytes onto it.
 * - steps:
 *     1. ECU: 48 6B, then 12 41 05 7B 86 after P1 max + 100 ms
 *     2. ISO9141_RecvResponse returns OBD_ERR_COMM_DATA_BYTE_NOT_RECVD
 *        with OBD_ERR_TIMEOUT_MAX
 ******************************************************************/
void test_neg_ex_ISO9141_RecvResponse_5(void)
{
    const uint8_t frame[] = { 0x48u, 0x6Bu, 0x12u, 0x41u, 0x05u, 0x7Bu, 0x86u };
    const uint32_t start = P2_TIME_MIN + DLFX_TIME_STEP;
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_ISO9141_RecvResponse_5.vcd");
    DLFX_SetUpLine();

    /* 1. */
    DLFX_LineWriteAt(start, frame[0]);
    DLFX_LineWriteAt(start + DLFX_ECU_BYTE_GAP, frame[1]);
    for (size_t idx = 2u; idx < sizeof(frame); idx++)
    {
        DLFX_LineWriteAt(start + P1_TIME_MAX + 100u + (uint32_t)idx * DLFX_ECU_BYTE_GAP, frame[idx]);
    }
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, DL_StartTimeout(&dlFx.link, P2_TIME_MAX).response);

    /* 2. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_DATA_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MAX, status.timeout);
}

/******************************************************************
 * - brief:       Connect_Verify needs a stored header
 * - description: The connect step fails when init left no header in
 *                the context, and passes once one is there.
 * - steps:
 *     1. Empty header -> OBD_ERR_PROTOCOL_NOT_FOUND
 *     2. Header 68 6A F1 -> OK
 ******************************************************************/
void test_neg_ex_ISO9141_Connect_Verify_0(void)
{
    UART_TRACE_Begin("test_neg_ex_ISO9141_Connect_Verify_0.vcd");
    DLFX_SetUpLine();

    /* 1. */
    dlFx.proto.header = (header_t){0};
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_PROTOCOL_NOT_FOUND, ISO9141_Connect_Verify(&dlFx.link).response);

    /* 2. */
    dlFx.proto.header = header_tester;
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, ISO9141_Connect_Verify(&dlFx.link).response);
    UART_TRACE_End();
}
