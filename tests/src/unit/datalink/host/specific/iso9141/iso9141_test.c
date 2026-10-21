/* ================================================ INCLUDES =============================================== */
#include "iso9141_test.h"
#include "datalink.h"
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
extern uint8_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen);
extern void L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);
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
    msg.cs = L2_ISO9141_ComputeChecksum(msg.header, msg.data, MSG_DATA_LEN);
    TEST_ASSERT_EQUAL_HEX8(msg_tester.cs, msg.cs);

    uint8_t aSentMsg[MSG_WIRE_LEN] = {0};
    size_t wireLen = 0;
    L2_ISO9141_PrepareMessage(&msg, aSentMsg, 2, &wireLen);
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

