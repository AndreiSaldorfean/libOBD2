/* ================================================ INCLUDES =============================================== */
#include "iso9141_test.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "datalink_test_vectors_000.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_trace.h"
#include "unity.h"
/* ================================================= MACROS ================================================ */
#define PBT_QUEUE_SIZE    4u
#define PBT_TRIALS        500u
#define MSG_DATA_LEN      7u  /* sid + 6 params, matches only-supported-protocol convention */
#define MSG_WIRE_LEN      11u /* 3 header + 7 data + 1 checksum */
#define MSG_QUEUE_SIZE    16u
#define ECU_BYTE_GAP      5u  /* ECU inter-byte time (P1), below the 10 ms poll step */

/* ============================================ LOCAL VARIABLES ============================================ */
/* Virtual line for the received-frame tests */
static virtTmrCtx_t lineTimer;
static virtUartLinkCtx_t lineUart;
static iso9141_ctx_t lineProto;
static dataLink_if_t lineLink;
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t ISO9141_ComputeChecksum(const header_t *header, const obd_data_t *data, size_t dataLen);
extern size_t ISO9141_SerializeMessage(const message_t *msg, uint8_t *frame, size_t dataLen);
extern obd_status_t ISO9141_SendMessage(dataLink_if_t *self, const uint8_t *frame, size_t frameLen);
extern obd_status_t ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t *len);
extern obd_status_t ISO9141_5BaudInit(dataLink_if_t *self, uint8_t *protocol);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* Fresh virtual line with an ISO9141 tester link on it */
static void setUpLine(void)
{
    lineTimer = (virtTmrCtx_t){ .timeStep = 10u };
    lineUart  = (virtUartLinkCtx_t){ .tmrHandle = &lineTimer };
    lineProto = (iso9141_ctx_t){ .header = header_tester, .protocol = ISO9141 };
    lineLink  = (dataLink_if_t)
    {
        .pProtocolCtx     = &lineProto,
        .pTimingOps       = &tmr_ops,
        .pTimingHandle    = &lineTimer,
        .pUartOps         = &uart_ops,
        .pTransportHandle = &lineUart,
    };
    TEST_ASSERT_TRUE(UART_Init(&lineUart));
}

/* Put an ECU frame on the line from just after P2 min, ECU_BYTE_GAP apart,
 * then rewind the clock and start P2 as a request that just ended would */
static void lineQueueEcuFrame(const uint8_t *bytes, size_t len)
{
    for (size_t idx = 0; idx < len; idx++)
    {
        lineTimer.timeNow = P2_TIME_MIN + lineTimer.timeStep + (uint32_t)idx * ECU_BYTE_GAP;
        UART_WriteByte(&lineUart, bytes[idx]);
    }
    lineTimer.timeNow = 0u;
    (void)DL_StartTimeout(&lineLink, P2_TIME_MAX);
}
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * Write then read one ISO9141 message on the virtual K-line.
 *   1. Tester sends 68 6A F1 01 05 00 00 00 00 00 C9 (P3 idle, P4 between bytes)
 *   2. The host UART is a loopback, so the frame stays on the line
 *   3. After P2 min the frame is read back with ISO9141_RecvMessage
 *   4. Header, data, length and checksum must match what was sent
 ******************************************************************/
void test_ISO9141_SendRecvMessage_000(void)
{
    dataLink_if_t *self = &dl_tester;
    message_t msg = msg_tester;
    msg.cs = ISO9141_ComputeChecksum(&msg.header, &msg.data, MSG_DATA_LEN);
    TEST_ASSERT_EQUAL_HEX8(msg_tester.cs, msg.cs);

    uint8_t aSentMsg[MSG_WIRE_LEN] = {0};
    size_t wireLen = ISO9141_SerializeMessage(&msg, aSentMsg, 2);
    TEST_ASSERT_EQUAL_size_t(6, wireLen);

    /* The vectors are static: start every run from a clean clock and an empty line.
     * Not TMR_Init(): it zeroes the whole timer, including timeStep. */
    tmr_ctx_tester.timeNow        = 0u;
    tmr_ctx_tester.timeoutTime    = 0u;
    tmr_ctx_tester.timeoutStarted = false;
    UART_TRACE_Begin("test_ISO9141_SendRecvMessage_000.vcd");
    TEST_ASSERT_TRUE(LIBOBD2_UART_Init(self));

    /* 1. Write */
    obd_status_t status = ISO9141_SendMessage(self, aSentMsg, wireLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 2. The response may not start before P2 min (counted from the end of the request) */
    LIBOBD2_TMR_Delay(self, P2_TIME_MIN);

    /* 3. Read */
    message_t recvdMsg = {0};
    size_t recvdLen = 0;
    status = ISO9141_RecvMessage(self, &recvdMsg, &recvdLen);
    UART_TRACE_End();

    /* 4. Compare */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(2, recvdLen);
    TEST_ASSERT_EQUAL_HEX8(msg.header.fmt, recvdMsg.header.fmt);
    TEST_ASSERT_EQUAL_HEX8(msg.header.trgAddr, recvdMsg.header.trgAddr);
    TEST_ASSERT_EQUAL_HEX8(msg.header.srcAddr, recvdMsg.header.srcAddr);
    TEST_ASSERT_EQUAL_HEX8(msg.data.sid, recvdMsg.data.sid);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(msg.data.param, recvdMsg.data.param, MSG_DATA_LEN - 1u);
    TEST_ASSERT_EQUAL_HEX8(msg.cs, recvdMsg.cs);
}

void test_ISO9141_5BaudInit_000(void)
{
    virtTmrCtx_t timer =
    {
        .timeNow        = 0u,
        .timeStep       = 10u,
        .timeoutTime    = 0u,
        .timeoutStarted = false,
    };
    virtUartLinkCtx_t uartCtx =
    {
        .tmrHandle = &timer,
    };
    dataLink_if_t self =
    {
        .pProtocolCtx     = &dl_ctx_tester,
        .pTimingOps       = &tmr_ops,
        .pTimingHandle    = &timer,
        .pUartOps         = &uart_ops,
        .pTransportHandle = &uartCtx,
    };
    uint8_t protocol = 0u;

    TEST_ASSERT_TRUE(UART_Init(&uartCtx));


    /* ECU responses in the order expected by the ISO9141-2 5-baud connect sequence:
     * sync, KB1, KB2, inverted target address (0xCC for 0x33).  The tester sends
     * its wake-up address on the same virtual loopback queue, so we preload the ECU
     * replies ahead of the self-generated 0x33 response to keep the reads ordered. */
    timer.timeNow = 300U + 2000U + ISO9141_W1_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x55u);
    timer.timeNow += ISO9141_W2_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x08u);
    timer.timeNow += ISO9141_W3_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x08u);
    timer.timeNow += 3*ISO9141_W4_TIME_MIN;
    UART_WriteByte(&uartCtx, (uint8_t)(~0x33u));

    // reset the time
    timer.timeNow = 0u;

    UART_TRACE_Begin("test_ISO9141_5BaudInit_000.vcd");
    timer.timeNow = 0u;
    obd_status_t status = ISO9141_5BaudInit(&self, &protocol);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_UINT8(ISO9141, protocol);
    TEST_ASSERT_EQUAL_HEX8(0x68u, dl_ctx_tester.header.fmt);
    TEST_ASSERT_EQUAL_HEX8(0x6Au, dl_ctx_tester.header.trgAddr);
    TEST_ASSERT_EQUAL_HEX8(0xF1u, dl_ctx_tester.header.srcAddr);
    TEST_ASSERT_EQUAL_HEX8(0x08u, dl_ctx_tester.kb1);
    TEST_ASSERT_EQUAL_HEX8(0x08u, dl_ctx_tester.kb2);
}


/******************************************************************
 * The ECU sends only the format byte of a response.
 *   1. The format byte arrives after P2 min, so the header read starts
 *   2. The target byte never arrives, so the P1 window expires
 *   3. RecvMessage must report OBD_ERR_COMM_DATA_BYTE_NOT_RECVD together
 *      with the P1 timeout, not a bare timeout with response OK
 ******************************************************************/
void test_ISO9141_RecvMessage_TargetByteTimeout(void)
{
    virtTmrCtx_t timer =
    {
        .timeNow        = 0u,
        .timeStep       = 10u,
        .timeoutTime    = 0u,
        .timeoutStarted = false,
    };
    virtUartLinkCtx_t uartCtx =
    {
        .tmrHandle = &timer,
    };
    dataLink_if_t self =
    {
        .pProtocolCtx     = &dl_ctx_tester,
        .pTimingOps       = &tmr_ops,
        .pTimingHandle    = &timer,
        .pUartOps         = &uart_ops,
        .pTransportHandle = &uartCtx,
    };
    message_t recvdMsg = {0};
    size_t recvdLen = 0;

    TEST_ASSERT_TRUE(UART_Init(&uartCtx));

    /* 1. Only the format byte is on the line, after P2 min */
    timer.timeNow = P2_TIME_MIN + timer.timeStep;
    UART_WriteByte(&uartCtx, 0x48u);

    /* P2 starts at the end of the (virtual) request */
    timer.timeNow = 0u;
    DL_StartTimeout(&self, P2_TIME_MAX);

    /* 2. + 3. */
    obd_status_t status = ISO9141_RecvMessage(&self, &recvdMsg, &recvdLen);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_DATA_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MAX, status.timeout);
    TEST_ASSERT_EQUAL_HEX8(0x48u, recvdMsg.header.fmt);
}

/******************************************************************
 * send_request accepts 1..7 data bytes (sid + up to 6 params).
 *   0 would underflow in SerializeMessage; 8 would overrun the frame buffer.
 *   A rejected request puts nothing on the line: no P3 wait, no bytes.
 ******************************************************************/
void test_ISO9141_SendRequest_InvalidDataLen(void)
{
    obd_data_t data = { .sid = 0x01u };

    tmr_ctx_tester.timeNow = 0u;

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, ISO9141_SendRequest(&dl_tester, &data, 0u).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, ISO9141_SendRequest(&dl_tester, &data, 8u).response);
    TEST_ASSERT_EQUAL_UINT32(0u, tmr_ctx_tester.timeNow);
}

/******************************************************************
 * The ECU completes the 5-baud handshake but sends keyword bytes
 * 0x12 0x34, which match neither ISO9141-2 (08 08) nor KWP2000 (94 94).
 * Init must fail with OBD_ERR_5BAUD_WRONG_KEYBYTES and store no header.
 ******************************************************************/
void test_ISO9141_5BaudInit_WrongKeyBytes(void)
{
    virtTmrCtx_t timer =
    {
        .timeNow        = 0u,
        .timeStep       = 10u,
        .timeoutTime    = 0u,
        .timeoutStarted = false,
    };
    virtUartLinkCtx_t uartCtx =
    {
        .tmrHandle = &timer,
    };
    iso9141_ctx_t protoCtx = {0};
    dataLink_if_t self =
    {
        .pProtocolCtx     = &protoCtx,
        .pTimingOps       = &tmr_ops,
        .pTimingHandle    = &timer,
        .pUartOps         = &uart_ops,
        .pTransportHandle = &uartCtx,
    };
    uint8_t protocol = 0xFFu;

    TEST_ASSERT_TRUE(UART_Init(&uartCtx));

    /* Same timing as test_ISO9141_5BaudInit_000, only the keyword bytes differ */
    timer.timeNow = 300U + 2000U + ISO9141_W1_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x55u);
    timer.timeNow += ISO9141_W2_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x12u);
    timer.timeNow += ISO9141_W3_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x34u);
    timer.timeNow += 3*ISO9141_W4_TIME_MIN;
    UART_WriteByte(&uartCtx, (uint8_t)(~0x33u));

    timer.timeNow = 0u;
    obd_status_t status = ISO9141_5BaudInit(&self, &protocol);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_5BAUD_WRONG_KEYBYTES, status.response);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, protocol);
    TEST_ASSERT_EQUAL_HEX8(0x00u, protoCtx.header.fmt);
}

/******************************************************************
 * Every public ISO9141 entry point rejects a NULL argument with
 * OBD_ERR_NULL_PTR before touching the line.
 ******************************************************************/
void test_ISO9141_Api_NullArgs(void)
{
    obd_data_t data = {0};
    size_t len = 0u;

    tmr_ctx_tester.timeNow = 0u;

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, ISO9141_Connect(&dl_tester, NULL).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, ISO9141_SendRequest(&dl_tester, NULL, 2u).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, ISO9141_RecvResponse(&dl_tester, NULL, &len).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, ISO9141_RecvResponse(&dl_tester, &data, NULL).response);
    TEST_ASSERT_EQUAL_UINT32(0u, tmr_ctx_tester.timeNow);
}

/******************************************************************
 * A negative ECU response is reported, not returned as data.
 *   ECU: 48 6B 12 7F 01 12 57   (sid 7F, rejected sid 01, NRC 12)
 ******************************************************************/
void test_ISO9141_RecvResponse_NegativeResponse(void)
{
    const uint8_t frame[] = { 0x48u, 0x6Bu, 0x12u, 0x7Fu, 0x01u, 0x12u, 0x57u };
    obd_data_t resp = {0};
    size_t len = 0u;

    setUpLine();
    lineQueueEcuFrame(frame, sizeof(frame));

    obd_status_t status = ISO9141_RecvResponse(&lineLink, &resp, &len);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_ECU_RESPONSE_7F, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
}

/******************************************************************
 * A frame whose checksum follows the header directly carries no sid
 * and is rejected.
 *   ECU: 48 6B 12 C5
 ******************************************************************/
void test_ISO9141_RecvResponse_EmptyFrame(void)
{
    const uint8_t frame[] = { 0x48u, 0x6Bu, 0x12u, 0xC5u };
    obd_data_t resp = {0};
    size_t len = 0u;

    setUpLine();
    lineQueueEcuFrame(frame, sizeof(frame));

    obd_status_t status = ISO9141_RecvResponse(&lineLink, &resp, &len);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_RECV_MSG_FAILED, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
}

/******************************************************************
 * A frame that fills the data field without a byte matching the
 * running checksum is rejected instead of overrunning message_t.
 *   ECU: 48 6B 12, then 13 x 01 (the sum never comes back to 01)
 ******************************************************************/
void test_ISO9141_RecvResponse_NoChecksum(void)
{
    uint8_t frame[3u + 13u] = { 0x48u, 0x6Bu, 0x12u };
    obd_data_t resp = {0};
    size_t len = 0u;

    for (size_t idx = 3u; idx < sizeof(frame); idx++)
    {
        frame[idx] = 0x01u;
    }

    setUpLine();
    lineQueueEcuFrame(frame, sizeof(frame));

    obd_status_t status = ISO9141_RecvResponse(&lineLink, &resp, &len);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_RECV_MSG_FAILED, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
}
