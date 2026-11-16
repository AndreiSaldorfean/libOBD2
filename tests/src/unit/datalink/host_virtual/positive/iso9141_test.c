/* ================================================ INCLUDES =============================================== */
#include "iso9141_test.h"
#include "dl_fixture.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "datalink_test_vectors_000.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_trace.h"
#include "unity.h"

/* ================================================= MACROS ================================================ */
#define MSG_DATA_LEN      7u  /* sid + 6 params, matches only-supported-protocol convention */
#define MSG_WIRE_LEN      11u /* 3 header + 7 data + 1 checksum */

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t ISO9141_ComputeChecksum(const header_t *header, const obd_data_t *data, size_t dataLen);
extern size_t ISO9141_SerializeMessage(const message_t *msg, uint8_t *frame, size_t dataLen);
extern obd_status_t ISO9141_SendMessage(dataLink_if_t *self, const uint8_t *frame, size_t frameLen);
extern obd_status_t ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t *len);
extern obd_status_t ISO9141_5BaudInit(dataLink_if_t *self, uint8_t *protocol);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       Write then read one ISO9141 message on the K-line
 * - description: Checksum, serialization, sending and receiving of a
 *                tester request; the host UART is a loopback, so the
 *                frame stays on the line and is read back.
 * - steps:
 *     1. Tester sends 68 6A F1 01 05 C9 (P3 idle, P4 between bytes)
 *     2. Wait P2 min (counted from the end of the request)
 *     3. Read the frame back with ISO9141_RecvMessage
 *     4. Header, data, length and checksum match what was sent
 ******************************************************************/
void test_pos_ISO9141_SendRecvMessage_0(void)
{
    dataLink_if_t *self = &dl_tester;
    message_t msg = msg_tester;
    uint8_t aSentMsg[MSG_WIRE_LEN] = {0};
    message_t recvdMsg = {0};
    size_t recvdLen = 0;
    size_t wireLen = 0;
    obd_status_t status = {0};

    msg.cs = ISO9141_ComputeChecksum(&msg.header, &msg.data, MSG_DATA_LEN);
    TEST_ASSERT_EQUAL_HEX8(msg_tester.cs, msg.cs);
    wireLen = ISO9141_SerializeMessage(&msg, aSentMsg, 2);
    TEST_ASSERT_EQUAL_size_t(6, wireLen);

    /* The vectors are static: start every run from a clean clock and an empty line.
     * Not TMR_Init(): it zeroes the whole timer, including timeStep. */
    tmr_ctx_tester.timeNow        = 0u;
    tmr_ctx_tester.timeoutTime    = 0u;
    tmr_ctx_tester.timeoutStarted = false;
    UART_TRACE_Begin("test_pos_ISO9141_SendRecvMessage_0.vcd");
    TEST_ASSERT_TRUE(LIBOBD2_UART_Init(self));

    /* 1. */
    status = ISO9141_SendMessage(self, aSentMsg, wireLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 2. */
    LIBOBD2_TMR_Delay(self, P2_TIME_MIN);

    /* 3. */
    status = ISO9141_RecvMessage(self, &recvdMsg, &recvdLen);
    UART_TRACE_End();

    /* 4. */
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

/******************************************************************
 * - brief:       ISO9141-2 5-baud init with keyword bytes 08 08
 * - description: The full wake-up handshake: address 0x33 at 5 baud,
 *                then sync, keyword bytes, ~KB2 and the inverted
 *                address, each inside its W1..W4 window.
 * - steps:
 *     1. ECU answers sync 55, KB1 08, KB2 08 and, after the tester's
 *        ~KB2, the inverted address CC
 *     2. ISO9141_5BaudInit returns OK with protocol ISO9141
 *     3. The 68 6A F1 header and kb 08 08 are stored
 ******************************************************************/
void test_pos_ISO9141_5BaudInit_0(void)
{
    const dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;
    uint8_t protocol = 0u;
    obd_status_t status = {0};

    DLFX_SetUpLine();
    dlFx.proto = (iso9141_ctx_t){0};

    /* 1. */
    DLFX_QueueEcu5BaudInit(&ecu);

    /* 2. */
    UART_TRACE_Begin("test_pos_ISO9141_5BaudInit_0.vcd");
    status = ISO9141_5BaudInit(&dlFx.link, &protocol);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_UINT8(ISO9141, protocol);

    /* 3. */
    TEST_ASSERT_EQUAL_HEX8(0x68u, dlFx.proto.header.fmt);
    TEST_ASSERT_EQUAL_HEX8(0x6Au, dlFx.proto.header.trgAddr);
    TEST_ASSERT_EQUAL_HEX8(0xF1u, dlFx.proto.header.srcAddr);
    TEST_ASSERT_EQUAL_HEX8(0x08u, dlFx.proto.kb1);
    TEST_ASSERT_EQUAL_HEX8(0x08u, dlFx.proto.kb2);
}

/******************************************************************
 * - brief:       Keyword bytes 94 94 select the KWP2000 header
 * - description: The 5-baud init completes with the KWP2000 header
 *                (C0 33 F1); the protocol stays ISO9141 until KWP2000
 *                is supported.
 * - steps:
 *     1. ECU answers 55 94 94 CC
 *     2. ISO9141_5BaudInit returns OK, protocol ISO9141
 *     3. Header C0 33, KB1 94 stored
 ******************************************************************/
void test_pos_ISO9141_5BaudInit_1(void)
{
    dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;
    uint8_t protocol = 0xFFu;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_pos_ISO9141_5BaudInit_1.vcd");
    DLFX_SetUpLine();

    /* 1. */
    ecu.kb1 = 0x94u;
    ecu.kb2 = 0x94u;
    DLFX_QueueEcu5BaudInit(&ecu);

    /* 2. */
    status = ISO9141_5BaudInit(&dlFx.link, &protocol);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_UINT8(ISO9141, protocol);

    /* 3. */
    TEST_ASSERT_EQUAL_HEX8(0xC0u, dlFx.proto.header.fmt);
    TEST_ASSERT_EQUAL_HEX8(0x33u, dlFx.proto.header.trgAddr);
    TEST_ASSERT_EQUAL_HEX8(0x94u, dlFx.proto.kb1);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A request with the maximum data length goes out
 * - description: 7 data bytes (sid + 6 params) is the largest request
 *                ISO9141 allows; it is accepted and the whole frame
 *                lands on the line.
 * - steps:
 *     1. ISO9141_SendRequest with sid 01 and 6 params, 7 bytes
 *     2. Returns OK
 *     3. 3 header + 7 data + 1 checksum = 11 bytes on the line, the
 *        last one is the checksum
 ******************************************************************/
void test_pos_ISO9141_SendRequest_0(void)
{
    const obd_data_t request = { .sid = 0x01u, .param = { 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u } };
    uint8_t byte = 0u;
    uint8_t sum = 0u;
    size_t cnt = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_pos_ISO9141_SendRequest_0.vcd");
    DLFX_SetUpLine();

    /* 1. + 2. */
    status = ISO9141_SendRequest(&dlFx.link, &request, MSG_DATA_LEN);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);

    /* 3. */
    while (OBD_UART_RX_OK == UART_RecvByte(&dlFx.uart, &byte))
    {
        if (cnt < (MSG_WIRE_LEN - 1u))
        {
            sum = (uint8_t)(sum + byte);
        }
        cnt++;
    }
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_size_t(MSG_WIRE_LEN, cnt);
    TEST_ASSERT_EQUAL_HEX8(sum, byte);
}
