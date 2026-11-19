/* ================================================ INCLUDES =============================================== */
#include "libobd2_test.h"
#include "libobd2_fixture.h"
#include "datalink.h"
#include "ecu_libobd2.h"
#include "iso15031_5.h"
#include "iso9141.h"
#include "libobd2.h"
#include "libobd2_priv.h"
#include "libobd2_services.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define CALID_LEN     16u
#define CALID_MSG_CNT 4u

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern const libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES];
extern bool LibOBD2_IsPidSupportedByEcu(uint8_t sid, uint8_t pid);
extern obd_status_t LibOBD2_DispatchSid(obd_ctx_t* ctx, const libobd2_service_t *libobd2Service, libobd2_data_t* response, size_t respCap, size_t * respLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       Connect to an ISO9141-2 ECU with LibOBD2_Init
 * - description: The library entry point runs the whole 5-baud init,
 *                leaves the data link bound to ISO9141 and reads which
 *                sids / PIDs the ECU supports.
 * - steps:
 *     1. Tester idles W5, then sends 0x33 at 5 baud
 *     2. ECU answers sync 0x55, KB1 0x08, KB2 0x08 and, after the
 *        tester's ~KB2, the inverted address 0xCC
 *     3. Tester: 68 6A F1 01 00 CS        ECU: 48 6B 12 41 00 08 00 00 00 CS
 *        (only PID $05 supported, no next range); $02 $05 $06 $08 $09
 *        are not answered
 *     4. LibOBD2_Init reports OK and binds the datalink to ISO9141
 *        with the 68 6A F1 header
 *     5. The context is connected; $01 and its PID $05 are supported,
 *        PID $0C and the silent sids are not
 ******************************************************************/
void test_pos_ex_LIBOBD2_Init_0(void)
{
    const uint8_t pidsResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_PIDS_SUPPORTED_01_20, 0x08u, 0x00u, 0x00u, 0x00u };
    obd_status_t status = {0};

    /* 2. */
    APPFX_SetUpLine();
    APPFX_QueueIso9141Init();

    /* 3. */
    (void)APPFX_QueueEcuFrame(APPFX_INIT_CONNECT_END + APPFX_REQ_DURATION(2u), pidsResp, sizeof(pidsResp));

    /* 1. */
    APPFX_SetEcuSupportsAll(false);
    appFx.ctx.connectionStatus = false;
    UART_TRACE_Begin("test_pos_ex_LIBOBD2_Init_0.vcd");
    status = LibOBD2_Init(&appFx.ctx);
    UART_TRACE_End();

    /* 4. */
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

    /* 5. */
    TEST_ASSERT_TRUE(appFx.ctx.connectionStatus);
    TEST_ASSERT_TRUE(LibOBD2_IsSidSupportedByEcu(SID_SHOW_CURRENT_DATA));
    TEST_ASSERT_TRUE(LibOBD2_IsLibobd2SidSupported(OBD_GET_COOLANT_TEMP));
    TEST_ASSERT_FALSE(LibOBD2_IsSidSupportedByEcu(SID_SHOW_FREEZE_FRAME_DATA));
    TEST_ASSERT_FALSE(LibOBD2_IsSidSupportedByEcu(SID_CONTROL_ONBOARD_SYSTEM));
    TEST_ASSERT_FALSE(LibOBD2_IsSidSupportedByEcu(SID_REQUEST_VEHICLE_INFO));
    TEST_ASSERT_FALSE(LibOBD2_IsLibobd2SidSupported(OBD_GET_VIN));
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
 *                A - 40 formula into an int16_t.
 * - steps:
 *     1. Tester: 68 6A F1 01 05 CS        ECU: 48 6B 12 41 05 7B CS
 *     2. The response is an int16_t, A - 40: 0x7B (123) -> 83 degC
 ******************************************************************/
void test_pos_ex_LIBOBD2_RequestService_2(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_COOLANT_TEMP, 0x7BU };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};
    int16_t temp = 0;

    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    /* 1. */
    UART_TRACE_Begin("test_pos_ex_LIBOBD2_RequestService_2.vcd");
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    UART_TRACE_End();

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(sizeof(temp), responseLen);
    memcpy(&temp, response, sizeof(temp));
    TEST_ASSERT_EQUAL_INT16(83, temp);
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
 * - description: TP_SEND, TP_RECV (one message), TP_READ_UNTIL_TO (the
 *                ECU stays silent, so it ends after P2 max with nothing
 *                more) and an unknown step (ignored), then TP_END.
 * - steps:
 *     1. ECU: 48 6B 12 41 05 7B CS, no echo of the request
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
    APPFX_SuppressEcho();
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

/******************************************************************
 * - brief:       Read which sids / PIDs the ECU supports
 * - description: Every sid with PIDs is probed with its PID 00; the
 *                ranges are followed while the last bit of a bitmap
 *                announces the next one. Bitmaps are A B C D with
 *                PID n * 0x20 + 1 in bit 7 of A.
 * - steps:
 *     1. ECU: 01 00 -> 41 00 BE 1F A8 13 (01 03..07 0C..10 11 13 15
 *                      1C 1F 20; 20 = next range)
 *             01 20 -> 41 20 80 00 00 01 (21, 40 = next range)
 *             01 40 -> 41 40 40 00 00 00 (42, no next range)
 *             02 00 -> 7F 02 11 (negative), 05 / 08 silent
 *             06 00 -> 46 00 FF 80 00 00 00 (01)
 *             09 00 -> 49 00 01 C0 00 00 00 (01 02)
 *     2. ReadSupportedPids returns OK after 8 requests: 01 00, 01 20,
 *        01 40, 02 00 00, 05 00 01, 06 00, 08 00 00 00 00 00 00, 09 00
 *     3. $01 $06 $09 are supported, $02 $05 $08 are not, sids without
 *        PIDs count as supported
 *     4. The $01 PIDs match the bitmaps, PID 00 always with its sid
 *     5. VIN ($09 01 + 02) and coolant ($01 05) are supported, the
 *        calibration ID ($09 03 + 04) is not
 ******************************************************************/
void test_pos_ex_LIBOBD2_ReadSupportedPids_0(void)
{
    const uint8_t pids01_00[] = { 0x41u, 0x00u, 0xBEu, 0x1Fu, 0xA8u, 0x13u };
    const uint8_t pids01_20[] = { 0x41u, 0x20u, 0x80u, 0x00u, 0x00u, 0x01u };
    const uint8_t pids01_40[] = { 0x41u, 0x40u, 0x40u, 0x00u, 0x00u, 0x00u };
    const uint8_t pids02_00[] = { 0x7Fu, 0x02u, 0x11u };
    const uint8_t pids06_00[] = { 0x46u, 0x00u, 0xFFu, 0x80u, 0x00u, 0x00u, 0x00u };
    const uint8_t pids09_00[] = { 0x49u, 0x00u, 0x01u, 0xC0u, 0x00u, 0x00u, 0x00u };
    const uint8_t expReqSids[] = { 0x01u, 0x01u, 0x01u, 0x02u, 0x05u, 0x06u, 0x08u, 0x09u };
    const uint8_t expReqPids[] = { 0x00u, 0x20u, 0x40u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    const uint8_t expReqP1[]   = { 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u };
    const size_t  expReqLens[] = { 2u, 2u, 2u, 3u, 3u, 2u, 7u, 2u };
    const uint8_t supported01[] = { 0x00u, 0x01u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x0Cu, 0x0Du, 0x0Eu,
                                    0x0Fu, 0x10u, 0x11u, 0x13u, 0x15u, 0x1Cu, 0x1Fu, 0x20u, 0x21u, 0x40u, 0x42u };
    static appFxScriptedEcu_t ecu;
    dataLink_if_t link = {0};
    obd_ctx_t ctx = {0};
    obd_status_t status = {0};
    size_t supportedCnt = 0u;

    /* 1. */
    APPFX_EcuReset(&ecu);
    APPFX_EcuAnswer(&ecu, 0x01u, 0x00u, pids01_00, sizeof(pids01_00));
    APPFX_EcuAnswer(&ecu, 0x01u, 0x20u, pids01_20, sizeof(pids01_20));
    APPFX_EcuAnswer(&ecu, 0x01u, 0x40u, pids01_40, sizeof(pids01_40));
    APPFX_EcuAnswer(&ecu, 0x02u, 0x00u, pids02_00, sizeof(pids02_00));
    APPFX_EcuAnswer(&ecu, 0x06u, 0x00u, pids06_00, sizeof(pids06_00));
    APPFX_EcuAnswer(&ecu, 0x09u, 0x00u, pids09_00, sizeof(pids09_00));
    link = APPFX_EcuLink(&ecu);
    ctx  = (obd_ctx_t){ .pDataLink = &link };
    APPFX_SetEcuSupportsAll(true);

    /* 2. */
    status = LibOBD2_ReadSupportedPids(&ctx);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(sizeof(expReqSids), ecu.reqCnt);
    for (size_t idx = 0u; idx < ecu.reqCnt; idx++)
    {
        TEST_ASSERT_EQUAL_HEX8(expReqSids[idx], ecu.reqs[idx].sid);
        TEST_ASSERT_EQUAL_HEX8(expReqPids[idx], ecu.reqs[idx].param[0]);
        TEST_ASSERT_EQUAL_HEX8(expReqP1[idx], ecu.reqs[idx].param[1]);
        TEST_ASSERT_EQUAL_size_t(expReqLens[idx], ecu.reqLens[idx]);
    }

    /* 3. */
    TEST_ASSERT_TRUE(LibOBD2_IsSidSupportedByEcu(SID_SHOW_CURRENT_DATA));
    TEST_ASSERT_TRUE(LibOBD2_IsSidSupportedByEcu(SID_REQUEST_VEHICLE_INFO));
    TEST_ASSERT_TRUE(LibOBD2_IsSidSupportedByEcu(SID_TEST_RESULTS_OTHER));
    TEST_ASSERT_FALSE(LibOBD2_IsSidSupportedByEcu(SID_SHOW_FREEZE_FRAME_DATA));
    TEST_ASSERT_FALSE(LibOBD2_IsSidSupportedByEcu(SID_TEST_RESULTS_O2_SENSORS));
    TEST_ASSERT_FALSE(LibOBD2_IsSidSupportedByEcu(SID_CONTROL_ONBOARD_SYSTEM));
    TEST_ASSERT_TRUE(LibOBD2_IsSidSupportedByEcu(SID_SHOW_STORED_DTCS));
    TEST_ASSERT_TRUE(LibOBD2_IsSidSupportedByEcu(SID_CLEAR_DTCS));

    /* 4. every $01 PID: supported exactly when listed */
    for (uint16_t pid = 0u; pid <= 0xFFu; pid++)
    {
        bool isListed = (NULL != memchr(supported01, (int)pid, sizeof(supported01)));

        TEST_ASSERT_EQUAL_MESSAGE(isListed, LibOBD2_IsPidSupportedByEcu(0x01u, (uint8_t)pid), "$01 PID");
        supportedCnt += isListed ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_size_t(sizeof(supported01), supportedCnt);
    TEST_ASSERT_FALSE(LibOBD2_IsPidSupportedByEcu(0x02u, 0x00u));
    TEST_ASSERT_TRUE(LibOBD2_IsPidSupportedByEcu(0x09u, 0x02u));
    TEST_ASSERT_FALSE(LibOBD2_IsPidSupportedByEcu(0x09u, 0x03u));
    TEST_ASSERT_TRUE(LibOBD2_IsPidSupportedByEcu(0x06u, 0x01u));
    TEST_ASSERT_FALSE(LibOBD2_IsPidSupportedByEcu(SID_SHOW_STORED_DTCS, 0x00u)); /* no PIDs at all */
    TEST_ASSERT_FALSE(LibOBD2_IsPidSupportedByEcu(0x06u, 0x02u));

    /* 5. */
    TEST_ASSERT_TRUE(LibOBD2_IsLibobd2SidSupported(OBD_GET_VIN));
    TEST_ASSERT_TRUE(LibOBD2_IsLibobd2SidSupported(OBD_GET_COOLANT_TEMP));
    TEST_ASSERT_FALSE(LibOBD2_IsLibobd2SidSupported(OBD_GET_CALID));
}

/******************************************************************
 * - brief:       Every PID range of a sid can be read
 * - description: An ECU announcing all 8 ranges of $01 (bit 0 of every
 *                bitmap set) is read up to range E0 and no further;
 *                PID FF, the last one, is found in range E0.
 * - steps:
 *     1. ECU: 01 00, 01 20 .. 01 E0 -> 41 pp 00 00 00 03 (pp+1F and
 *        pp+20 supported); every other sid silent
 *     2. ReadSupportedPids returns OK, with 8 $01 requests 00 .. E0
 *     3. PIDs 1F 20 3F 40 .. FF are supported, 01 and FE are not
 ******************************************************************/
void test_pos_ex_LIBOBD2_ReadSupportedPids_1(void)
{
    static appFxScriptedEcu_t ecu;
    dataLink_if_t link = {0};
    obd_ctx_t ctx = {0};
    obd_status_t status = {0};

    /* 1. */
    APPFX_EcuReset(&ecu);
    for (uint16_t pid = 0u; pid <= 0xE0u; pid += 0x20u)
    {
        const uint8_t resp[] = { 0x41u, (uint8_t)pid, 0x00u, 0x00u, 0x00u, 0x03u };
        APPFX_EcuAnswer(&ecu, 0x01u, (uint8_t)pid, resp, sizeof(resp));
    }
    link = APPFX_EcuLink(&ecu);
    ctx  = (obd_ctx_t){ .pDataLink = &link };

    /* 2. */
    status = LibOBD2_ReadSupportedPids(&ctx);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    for (size_t idx = 0u; idx < 8u; idx++)
    {
        TEST_ASSERT_EQUAL_HEX8(0x01u, ecu.reqs[idx].sid);
        TEST_ASSERT_EQUAL_HEX8((uint8_t)(idx * 0x20u), ecu.reqs[idx].param[0]);
    }
    TEST_ASSERT_NOT_EQUAL_HEX8(0x01u, ecu.reqs[8].sid);

    /* 3. */
    TEST_ASSERT_TRUE(LibOBD2_IsPidSupportedByEcu(0x01u, 0x1Fu));
    TEST_ASSERT_TRUE(LibOBD2_IsPidSupportedByEcu(0x01u, 0x20u));
    TEST_ASSERT_TRUE(LibOBD2_IsPidSupportedByEcu(0x01u, 0x3Fu));
    TEST_ASSERT_TRUE(LibOBD2_IsPidSupportedByEcu(0x01u, 0x40u));
    TEST_ASSERT_TRUE(LibOBD2_IsPidSupportedByEcu(0x01u, 0xFFu));
    TEST_ASSERT_FALSE(LibOBD2_IsPidSupportedByEcu(0x01u, 0x01u));
    TEST_ASSERT_FALSE(LibOBD2_IsPidSupportedByEcu(0x01u, 0xFEu));
}

/******************************************************************
 * - brief:       Everything the ECU simulator answers is allowed
 * - description: Discovery against the ECU simulator's own answer
 *                table: every library service whose requests the
 *                simulator answers must pass the supported check, so
 *                the bench never refuses a service it could serve.
 * - steps:
 *     1. Scripted ECU answering from the simulator (ECU_LibOBD2_GetAnswer)
 *     2. ReadSupportedPids returns OK, every sid with PIDs is supported
 *     3. Every libobd2_sid_t whose requests all have a simulator
 *        answer is supported
 ******************************************************************/
void test_pos_ex_LIBOBD2_ReadSupportedPids_2(void)
{
    static appFxScriptedEcu_t ecu;
    dataLink_if_t link = {0};
    obd_ctx_t ctx = {0};
    obd_status_t status = {0};
    size_t checkedCnt = 0u;

    /* 1. */
    APPFX_EcuReset(&ecu);
    ecu.useSimulator = true;
    link = APPFX_EcuLink(&ecu);
    ctx  = (obd_ctx_t){ .pDataLink = &link };

    /* 2. */
    status = LibOBD2_ReadSupportedPids(&ctx);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    for (uint8_t sid = SID_SHOW_CURRENT_DATA; sid <= SID_PERMANENT_DTCS; sid++)
    {
        TEST_ASSERT_TRUE_MESSAGE(LibOBD2_IsSidSupportedByEcu(sid), "sid not supported by the simulator");
    }

    /* 3. */
    for (size_t obdSid = 0u; obdSid < (size_t)OBD2_MAX_NUM_SERVICES; obdSid++)
    {
        const libobd2_service_t *service = &libobd2Services[obdSid];
        bool isAnswered = true;

        for (size_t reqIdx = 0u; (reqIdx < LIBOBD2_MAX_SREQ) && (0u < service->serviceRequests[reqIdx].dataLen); reqIdx++)
        {
            const obd_data_t *req = &service->serviceRequests[reqIdx].data;
            isAnswered = isAnswered && (NULL != ECU_LibOBD2_GetAnswer(req->sid, req->param[0]));
        }

        if (isAnswered)
        {
            char msg[48];
            (void)snprintf(msg, sizeof(msg), "service %u (sid %02X pid %02X) refused", (unsigned)obdSid,
                           service->serviceRequests[0].data.sid, service->serviceRequests[0].data.param[0]);
            TEST_ASSERT_TRUE_MESSAGE(LibOBD2_IsLibobd2SidSupported((libobd2_sid_t)obdSid), msg);
            checkedCnt++;
        }
    }
    TEST_ASSERT_GREATER_THAN_size_t(0u, checkedCnt);
}
