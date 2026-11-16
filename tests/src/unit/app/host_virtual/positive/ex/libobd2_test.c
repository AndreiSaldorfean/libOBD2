/* ================================================ INCLUDES =============================================== */
#include "libobd2_test.h"
#include "libobd2_fixture.h"
#include "datalink.h"
#include "iso15031_5.h"
#include "iso9141.h"
#include "libobd2.h"
#include "libobd2_services.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <string.h>

/* ================================================= MACROS ================================================ */
#define CALID_LEN     16u
#define CALID_MSG_CNT 4u

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern const libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES];
extern obd_status_t LibOBD2_DispatchSid(obd_ctx_t* ctx, const libobd2_service_t *libobd2Service, libobd2_data_t* response, size_t respCap, size_t * respLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       Connect to an ISO9141-2 ECU with LibOBD2_Init
 * - description: The library entry point runs the whole 5-baud init
 *                and leaves the data link bound to ISO9141.
 * - steps:
 *     1. Tester idles W5, then sends 0x33 at 5 baud
 *     2. ECU answers sync 0x55, KB1 0x08, KB2 0x08 and, after the
 *        tester's ~KB2, the inverted address 0xCC
 *     3. LibOBD2_Init reports OK and binds the datalink to ISO9141
 *        with the 68 6A F1 header
 ******************************************************************/
void test_pos_ex_LIBOBD2_Init_0(void)
{
    obd_status_t status = {0};

    APPFX_SetUpLine();
    /* nothing is known about the protocol before the connect */
    appFx.proto = (iso9141_ctx_t){0};
    appFx.link.connect       = NULL;
    appFx.link.send_request  = NULL;
    appFx.link.recv_response = NULL;

    /* 2. Same ECU timing as test_pos_ex_ISO9141_5BaudInit_0: W5 + 10 bits at 5 baud, then W1..W4 */
    appFx.timer.timeNow = ISO9141_W5_TIME_MIN + 2000u + ISO9141_W1_TIME_MIN;
    UART_WriteByte(&appFx.uart, 0x55u);
    appFx.timer.timeNow += ISO9141_W2_TIME_MIN;
    UART_WriteByte(&appFx.uart, 0x08u);
    appFx.timer.timeNow += ISO9141_W3_TIME_MIN;
    UART_WriteByte(&appFx.uart, 0x08u);
    appFx.timer.timeNow += 3u * ISO9141_W4_TIME_MIN;
    UART_WriteByte(&appFx.uart, (uint8_t)(~0x33u));
    appFx.timer.timeNow = 0u;

    /* 1. */
    UART_TRACE_Begin("test_pos_ex_LIBOBD2_Init_0.vcd");
    status = LibOBD2_Init(&appFx.ctx);
    UART_TRACE_End();

    /* 3. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_PTR(ISO9141_Connect, appFx.link.connect);
    TEST_ASSERT_EQUAL_PTR(ISO9141_SendRequest, appFx.link.send_request);
    TEST_ASSERT_EQUAL_PTR(ISO9141_RecvResponse, appFx.link.recv_response);
    TEST_ASSERT_EQUAL_HEX8(0x68u, appFx.proto.header.fmt);
    TEST_ASSERT_EQUAL_HEX8(0x6Au, appFx.proto.header.trgAddr);
    TEST_ASSERT_EQUAL_HEX8(0xF1u, appFx.proto.header.srcAddr);
    TEST_ASSERT_EQUAL_HEX8(0x08u, appFx.proto.kb1);
    TEST_ASSERT_EQUAL_HEX8(0x08u, appFx.proto.kb2);
}

/******************************************************************
 * - brief:       Read the VIN (SID $09) over the virtual K-line
 * - description: The VIN comes in five messages after a message-count
 *                answer; the library stitches the data bytes together.
 * - steps:
 *     1. Tester: 68 6A F1 09 01 CS        ECU: 48 6B 12 49 01 05 CS
 *     2. Tester: 68 6A F1 09 02 CS        ECU: 5 x 48 6B 12 49 02 nn D1..D4 CS
 *     3. The response is D1..D4 of every message, in order:
 *        3 pad bytes followed by the 17 VIN characters
 ******************************************************************/
void test_pos_ex_LIBOBD2_RequestService_0(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    APPFX_SetUpLine();
    APPFX_QueueVehicleInfo(PID_09_VIN_MESSAGE_COUNT, PID_09_VIN, appFxVin, APPFX_VIN_MSG_CNT, APPFX_VIN_MSG_CNT);

    /* 1. + 2. */
    UART_TRACE_Begin("test_pos_ex_LIBOBD2_RequestService_0.vcd");
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_VIN, response, &responseLen);
    UART_TRACE_End();

    /* 3. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(sizeof(appFxVin), responseLen);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(appFxVin, response, sizeof(appFxVin));
    TEST_ASSERT_EQUAL_CHAR_ARRAY("1HGCM82633A004352", &response[sizeof(appFxVin) - APPFX_VIN_LEN], APPFX_VIN_LEN);
}

/******************************************************************
 * - brief:       Read the calibration ID (SID $09) over the K-line
 * - description: Same multi-message exchange as the VIN, with the
 *                CALID PIDs and four messages.
 * - steps:
 *     1. Tester: 68 6A F1 09 03 CS        ECU: 48 6B 12 49 03 04 CS
 *     2. Tester: 68 6A F1 09 04 CS        ECU: 4 x 48 6B 12 49 04 nn D1..D4 CS
 *     3. The response is the 16 calibration ID bytes, in order
 ******************************************************************/
void test_pos_ex_LIBOBD2_RequestService_1(void)
{
    const uint8_t calId[CALID_LEN] = { 'J', 'M', 'B', '*', '3', '6', '7', '6',
                                       '1', '5', '0', '0', 0x00, 0x00, 0x00, 0x00 };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    APPFX_SetUpLine();
    APPFX_QueueVehicleInfo(PID_09_CALID_MESSAGE_COUNT, PID_09_CALID, calId, CALID_MSG_CNT, CALID_MSG_CNT);

    /* 1. + 2. */
    UART_TRACE_Begin("test_pos_ex_LIBOBD2_RequestService_1.vcd");
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_CALID, response, &responseLen);
    UART_TRACE_End();

    /* 3. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(CALID_LEN, responseLen);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(calId, response, CALID_LEN);
}

/******************************************************************
 * - brief:       Read the engine coolant temperature (SID $01 PID $05)
 * - description: A single-message live data request decoded with the
 *                A - 40 formula.
 * - steps:
 *     1. Tester: 68 6A F1 01 05 CS        ECU: 48 6B 12 41 05 7B CS
 *     2. The response is one byte, A - 40: 0x7B (123) -> 83 degC
 ******************************************************************/
void test_pos_ex_LIBOBD2_RequestService_2(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_COOLANT_TEMP, 0x7BU };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    /* 1. */
    UART_TRACE_Begin("test_pos_ex_LIBOBD2_RequestService_2.vcd");
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    UART_TRACE_End();

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(1, responseLen);
    TEST_ASSERT_EQUAL_UINT8(83u, response[0]);
}

/******************************************************************
 * - brief:       A single-message service keeps the message length
 * - description: DispatchSid stores the length of the ECU message as
 *                it came: 41 05 7B is 3 data bytes (sid + 2 params).
 * - steps:
 *     1. ECU: 48 6B 12 41 05 7B CS
 *     2. DispatchSid returns OK with one message of 3 data bytes
 ******************************************************************/
void test_pos_ex_LIBOBD2_DispatchSid_0(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_COOLANT_TEMP, 0x7BU };
    libobd2_data_t responseBuffer[2] = {0};
    size_t respLen = 0;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    /* 2. */
    status = LibOBD2_DispatchSid(&appFx.ctx, &libobd2Services[OBD_GET_COOLANT_TEMP], responseBuffer, 2u, &respLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(1, respLen);
    TEST_ASSERT_EQUAL_size_t(sizeof(ecuResp), responseBuffer[0].dataLen);
}

/******************************************************************
 * - brief:       Transfer protocol steps no table service uses yet
 * - description: TP_SEND, TP_RECV (one message), TP_READ_UNTIL_TO (not
 *                implemented, does nothing) and an unknown step
 *                (ignored), then TP_END.
 * - steps:
 *     1. ECU: 48 6B 12 41 05 7B CS
 *     2. DispatchSid with { SEND, RECV, READ_UNTIL_TO, 0x7F, END }
 *     3. OK with the one received message
 ******************************************************************/
void test_pos_ex_LIBOBD2_DispatchSid_1(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_COOLANT_TEMP, 0x7BU };
    const libobd2_service_t service =
    {
        .sidSize          = OBD_LONG_SID,
        .serviceRequests  = SREQ_GET_COOLANT_TEMP,
        .transferProtocol = { TP_SEND, TP_RECV, TP_READ_UNTIL_TO, 0x7Fu, TP_END },
    };
    libobd2_data_t responseBuffer[2] = {0};
    size_t respLen = 0;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_pos_ex_LIBOBD2_DispatchSid_1.vcd");

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    /* 2. */
    status = LibOBD2_DispatchSid(&appFx.ctx, &service, responseBuffer, 2u, &respLen);

    /* 3. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(1, respLen);
    TEST_ASSERT_EQUAL_HEX8(ecuResp[0], responseBuffer[0].data.sid);
    TEST_ASSERT_EQUAL_size_t(sizeof(ecuResp), responseBuffer[0].dataLen);
    UART_TRACE_End();
}
