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

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern obd_status_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen, uint8_t *cs);
extern obd_status_t L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);
extern obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
extern obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t *len);
extern obd_status_t L2_ISO9141_5BaudInit(dataLink_if_t *self, uint8_t *protocol);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * Write then read one ISO9141 message on the virtual K-line.
 *   1. Tester sends 68 6A F1 01 05 00 00 00 00 00 C9 (P3 idle, P4 between bytes)
 *   2. The host UART is a loopback, so the frame stays on the line
 *   3. After P2 min the frame is read back with L2_ISO9141_RecvMessage
 *   4. Header, data, length and checksum must match what was sent
 ******************************************************************/
void test_ISO9141_SendRecvMessage_000(void)
{
    dataLink_if_t *self = &dl_tester;
    message_t msg = msg_tester;
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, L2_ISO9141_ComputeChecksum(msg.header, msg.data, MSG_DATA_LEN, &msg.cs).response);
    TEST_ASSERT_EQUAL_HEX8(msg_tester.cs, msg.cs);

    uint8_t aSentMsg[MSG_WIRE_LEN] = {0};
    size_t wireLen = 0;
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, L2_ISO9141_PrepareMessage(&msg, aSentMsg, 2, &wireLen).response);
    TEST_ASSERT_EQUAL_size_t(6, wireLen);

    /* The vectors are static: start every run from a clean clock and an empty line.
     * Not LIBOBD2_TMR_Init(): it zeroes the whole timer, including timeStep. */
    tmr_ctx_tester.timeNow        = 0u;
    tmr_ctx_tester.timeoutTime    = 0u;
    tmr_ctx_tester.timeoutStarted = false;
    UART_TRACE_Begin("test_ISO9141_SendRecvMessage_000.vcd");
    TEST_ASSERT_TRUE(LIBOBD_TransportInit(self));

    /* 1. Write */
    obd_status_t status = L2_ISO9141_SendMessage(self, aSentMsg, wireLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 2. The response may not start before P2 min (counted from the end of the request) */
    LIBOBD_Delay(self, P2_TIME_MIN);

    /* 3. Read */
    message_t recvdMsg = {0};
    size_t recvdLen = 0;
    status = L2_ISO9141_RecvMessage(self, &recvdMsg, &recvdLen);
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

    TEST_ASSERT_TRUE(LIBOBD2_UART_Init(&uartCtx));


    /* ECU responses in the order expected by the ISO9141-2 5-baud connect sequence:
     * sync, KB1, KB2, inverted target address (0xCC for 0x33).  The tester sends
     * its wake-up address on the same virtual loopback queue, so we preload the ECU
     * replies ahead of the self-generated 0x33 response to keep the reads ordered. */
    timer.timeNow = 300U + 2000U + ISO9141_W1_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x55u);
    timer.timeNow += ISO9141_W2_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x08u);
    timer.timeNow += ISO9141_W3_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x08u);
    timer.timeNow += 3*ISO9141_W4_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, (uint8_t)(~0x33u));

    // reset the time
    timer.timeNow = 0u;

    UART_TRACE_Begin("test_ISO9141_5BaudInit_000.vcd");
    timer.timeNow = 0u;
    obd_status_t status = L2_ISO9141_5BaudInit(&self, &protocol);
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
 *   3. RecvMessage must report OBD_ERR_COMM_TRGT_BYTE_NOT_RECVD together
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

    TEST_ASSERT_TRUE(LIBOBD2_UART_Init(&uartCtx));

    /* 1. Only the format byte is on the line, after P2 min */
    timer.timeNow = P2_TIME_MIN + timer.timeStep;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x48u);

    /* P2 starts at the end of the (virtual) request */
    timer.timeNow = 0u;
    DL_StartTimeout(&self, P2_TIME_MAX);

    /* 2. + 3. */
    obd_status_t status = L2_ISO9141_RecvMessage(&self, &recvdMsg, &recvdLen);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_TRGT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MAX, status.timeout);
    TEST_ASSERT_EQUAL_HEX8(0x48u, recvdMsg.header.fmt);
}

/******************************************************************
 * Every public ISO9141 entry point rejects a NULL data link with
 * OBD_ERR_NULL_PTR before touching the line.
 ******************************************************************/
void test_ISO9141_Api_NullLink(void)
{
    uint8_t protocol = 0u;
    obd_data_t data = {0};
    size_t len = 0;

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, l2_iso9141_connect(NULL, &protocol).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, l2_iso9141_send_request(NULL, &data, 2u).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, l2_iso9141_recv_response(NULL, &data, &len).response);
}

/******************************************************************
 * send_request accepts 1..7 data bytes (sid + up to 6 params).
 *   0 used to underflow in PrepareMessage; 8 would overrun the frame buffer.
 ******************************************************************/
void test_ISO9141_SendRequest_InvalidDataLen(void)
{
    obd_data_t data = { .sid = 0x01u };

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, l2_iso9141_send_request(&dl_tester, &data, 0u).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, l2_iso9141_send_request(&dl_tester, &data, 8u).response);
}

/******************************************************************
 * PrepareMessage with dataLen 0 is rejected and leaves the buffer as it was.
 ******************************************************************/
void test_ISO9141_PrepareMessage_ZeroDataLen(void)
{
    message_t msg = msg_tester;
    uint8_t aSentMsg[MSG_WIRE_LEN];
    uint8_t expected[MSG_WIRE_LEN];
    size_t wireLen = 0xA5u;

    memset(aSentMsg, 0xEEu, sizeof(aSentMsg));
    memset(expected, 0xEEu, sizeof(expected));

    obd_status_t status = L2_ISO9141_PrepareMessage(&msg, aSentMsg, 0u, &wireLen);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, status.response);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, aSentMsg, MSG_WIRE_LEN);
    TEST_ASSERT_EQUAL_size_t(0xA5u, wireLen);
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
    l2_iso9141_ctx_t protoCtx = {0};
    dataLink_if_t self =
    {
        .pProtocolCtx     = &protoCtx,
        .pTimingOps       = &tmr_ops,
        .pTimingHandle    = &timer,
        .pUartOps         = &uart_ops,
        .pTransportHandle = &uartCtx,
    };
    uint8_t protocol = 0xFFu;

    TEST_ASSERT_TRUE(LIBOBD2_UART_Init(&uartCtx));

    /* Same timing as test_ISO9141_5BaudInit_000, only the keyword bytes differ */
    timer.timeNow = 300U + 2000U + ISO9141_W1_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x55u);
    timer.timeNow += ISO9141_W2_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x12u);
    timer.timeNow += ISO9141_W3_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x34u);
    timer.timeNow += 3*ISO9141_W4_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, (uint8_t)(~0x33u));

    timer.timeNow = 0u;
    obd_status_t status = L2_ISO9141_5BaudInit(&self, &protocol);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_5BAUD_WRONG_KEYBYTES, status.response);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, protocol);
    TEST_ASSERT_EQUAL_HEX8(0x00u, protoCtx.header.fmt);
}
