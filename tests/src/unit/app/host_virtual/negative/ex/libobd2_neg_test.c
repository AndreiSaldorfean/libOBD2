/* ================================================ INCLUDES =============================================== */
#include "libobd2_neg_test.h"
#include "libobd2_fixture.h"
#include "datalink.h"
#include "fault_injection.h"
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
#define NO_LEN 0xA5u /* responseLen marker: the call did not write it */

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern obd_status_t LibOBD2_DecodeResponse(const libobd2_data_t *request, const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       LibOBD2_Init rejects a NULL context
 * - description: Init validates its context and fails with
 *                OBD_ERR_NULL_PTR instead of dereferencing NULL.
 * - steps:
 *     1. LibOBD2_Init(NULL) returns OBD_ERR_NULL_PTR
 ******************************************************************/
void test_neg_ex_LIBOBD2_Init_0(void)
{
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_Init(NULL).response);
}

/******************************************************************
 * - brief:       LibOBD2_Init rejects a context without a data link
 * - description: A context whose pDataLink is NULL is refused with
 *                OBD_ERR_NULL_PTR.
 * - steps:
 *     1. LibOBD2_Init on a context with pDataLink NULL returns
 *        OBD_ERR_NULL_PTR
 ******************************************************************/
void test_neg_ex_LIBOBD2_Init_1(void)
{
    obd_ctx_t nullLinkCtx = { .pDataLink = NULL };

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_Init(&nullLinkCtx).response);
}

/******************************************************************
 * - brief:       No ECU on the line: LibOBD2_Init fails
 * - description: The 5-baud wake-up goes out but nobody answers; the
 *                connect error comes back to the caller and the link
 *                stays unbound.
 * - steps:
 *     1. Empty line, nothing bound on the link
 *     2. LibOBD2_Init returns OBD_ERR_5BAUD_SYNC_NOT_RECVD
 *     3. send/recv still unbound
 ******************************************************************/
void test_neg_ex_LIBOBD2_Init_2(void)
{
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_LIBOBD2_Init_2.vcd");

    /* 1. */
    APPFX_SetUpLine();
    appFx.proto = (iso9141_ctx_t){0};
    appFx.link.connect       = NULL;
    appFx.link.send_request  = NULL;
    appFx.link.recv_response = NULL;

    /* 2. */
    status = LibOBD2_Init(&appFx.ctx);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_5BAUD_SYNC_NOT_RECVD, status.response);

    /* 3. */
    TEST_ASSERT_NULL(appFx.link.send_request);
    TEST_ASSERT_NULL(appFx.link.recv_response);
}

/******************************************************************
 * - brief:       RequestService rejects a NULL response buffer
 * - description: The output buffer is validated before anything goes
 *                on the line.
 * - steps:
 *     1. RequestService(VIN, response NULL) returns OBD_ERR_NULL_PTR
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_0(void)
{
    size_t responseLen = 0;

    APPFX_SetUpLine();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR,
                            LibOBD2_RequestService(&appFx.ctx, OBD_GET_VIN, NULL, &responseLen).response);
}

/******************************************************************
 * - brief:       RequestService rejects a NULL response length
 * - description: The length output is validated before anything goes
 *                on the line.
 * - steps:
 *     1. RequestService(VIN, responseLen NULL) returns OBD_ERR_NULL_PTR
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_1(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};

    APPFX_SetUpLine();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR,
                            LibOBD2_RequestService(&appFx.ctx, OBD_GET_VIN, response, NULL).response);
}

/******************************************************************
 * - brief:       An unknown service is rejected up front
 * - description: A service id past the table is refused with
 *                OBD_ERR_INVALID_PARAM before anything goes on the
 *                line.
 * - steps:
 *     1. RequestService(OBD2_MAX_NUM_SERVICES) returns OBD_ERR_INVALID_PARAM
 *     2. No response data, the clock never moved
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_2(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    APPFX_SetUpLine();

    /* 1. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD2_MAX_NUM_SERVICES, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, status.response);

    /* 2. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
    TEST_ASSERT_EQUAL_UINT32(0u, appFx.timer.timeNow);
}

/******************************************************************
 * - brief:       RequestService rejects a NULL context
 * - description: A NULL context is refused before anything goes on
 *                the line.
 * - steps:
 *     1. RequestService(NULL, VIN) returns OBD_ERR_NULL_PTR
 *     2. No response data
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_3(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;

    /* 1. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR,
                            LibOBD2_RequestService(NULL, OBD_GET_VIN, response, &responseLen).response);

    /* 2. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
}

/******************************************************************
 * - brief:       RequestService rejects a context without a link
 * - description: A context whose pDataLink is NULL is refused before
 *                anything goes on the line.
 * - steps:
 *     1. RequestService on a context with pDataLink NULL returns
 *        OBD_ERR_NULL_PTR
 *     2. No response data
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_4(void)
{
    obd_ctx_t noLinkCtx = { .pDataLink = NULL };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;

    UART_TRACE_Begin("test_neg_ex_LIBOBD2_RequestService_4.vcd");

    /* 1. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR,
                            LibOBD2_RequestService(&noLinkCtx, OBD_GET_VIN, response, &responseLen).response);

    /* 2. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A request before LibOBD2_Init fails cleanly
 * - description: The link has no protocol bound yet (Init never ran or
 *                failed): the request fails with OBD_ERR_NULL_PTR and
 *                nothing goes on the line.
 * - steps:
 *     1. Link without send/recv functions
 *     2. RequestService(coolant) returns OBD_ERR_NULL_PTR
 *     3. No response data, the clock never moved
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_5(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    appFx.link.send_request  = NULL;
    appFx.link.recv_response = NULL;

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
    TEST_ASSERT_EQUAL_UINT32(0u, appFx.timer.timeNow);
}

/******************************************************************
 * - brief:       The ECU announces more messages than fit
 * - description: The message count answer says 255 VIN messages; the
 *                request stops before reading any of them.
 * - steps:
 *     1. Tester: 68 6A F1 09 01 CS      ECU: 48 6B 12 49 01 FF CS
 *     2. RequestService returns OBD_ERR_APP_TOO_MANY_MSGS
 *     3. No response data
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_6(void)
{
    const uint8_t cntResp[] = { SID_REQUEST_VEHICLE_INFO + 0x40u, PID_09_VIN_MESSAGE_COUNT, 0xFFu };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), cntResp, sizeof(cntResp));

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_VIN, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_APP_TOO_MANY_MSGS, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
}

/******************************************************************
 * - brief:       Answers the decoder does not know are not supported
 * - description: ECU answers the decoder cannot interpret are reported
 *                as OBD_NOT_SUPPORTED with no data, never as OK.
 * - steps:
 *     1. No message at all for a $01 request
 *     2. An unknown service (positive response to $05)
 *     3. An unknown $01 PID
 *     4. An unknown $09 PID
 *     5. An answer for another service than the request ($01 asked,
 *        $09 answered)
 ******************************************************************/
void test_neg_ex_LIBOBD2_DecodeResponse_0(void)
{
    const libobd2_data_t requests[] =
    {
        { .data = { .sid = SID_TEST_RESULTS_O2_SENSORS, .param = { 0x01u } }, .dataLen = 2u },
        { .data = { .sid = SID_SHOW_CURRENT_DATA,       .param = { 0xFFu } }, .dataLen = 2u },
        { .data = { .sid = SID_REQUEST_VEHICLE_INFO,    .param = { 0xFFu } }, .dataLen = 2u },
        { .data = { .sid = SID_SHOW_CURRENT_DATA,       .param = { PID_09_VIN } }, .dataLen = 2u },
    };
    const libobd2_data_t answers[] =
    {
        { .data = { .sid = SID_TEST_RESULTS_O2_SENSORS + 0x40u, .param = { 0x01u, 0x00u } }, .dataLen = 3u },
        { .data = { .sid = SID_SHOW_CURRENT_DATA + 0x40u,    .param = { 0xFFu, 0x00u } }, .dataLen = 3u },
        { .data = { .sid = SID_REQUEST_VEHICLE_INFO + 0x40u, .param = { 0xFFu, 0x01u } }, .dataLen = 3u },
        { .data = { .sid = SID_REQUEST_VEHICLE_INFO + 0x40u, .param = { PID_09_VIN, 0x01u, 'W', 'V', 'W', 'Z' } }, .dataLen = 7u },
    };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = NO_LEN;

    UART_TRACE_Begin("test_neg_ex_LIBOBD2_DecodeResponse_0.vcd");

    /* 1. */
    TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, LibOBD2_DecodeResponse(&requests[1], answers, 0u, response, &responseLen).response);
    TEST_ASSERT_EQUAL_size_t(0, responseLen);

    /* 2. - 5. */
    for (size_t idx = 0; idx < (sizeof(answers) / sizeof(answers[0])); idx++)
    {
        responseLen = NO_LEN;
        TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, LibOBD2_DecodeResponse(&requests[idx], &answers[idx], 1u, response, &responseLen).response);
        TEST_ASSERT_EQUAL_size_t(0, responseLen);
    }
    UART_TRACE_End();
}

/******************************************************************
 * - brief:       A framing error inside the VIN stops the request
 * - description: One broken byte in the 3rd VIN message ends the
 *                request with the UART error and no data.
 *                Line bytes: count answer 0..6, VIN messages 7..17,
 *                18..28, 29..39; byte 34 (message number of the 3rd
 *                message) gets the error.
 * - steps:
 *     1. Inject a framing error on line byte 34
 *     2. Queue the VIN exchange
 *     3. RequestService returns OBD_ERR_UART_FRAMING with no data
 *     4. Exactly one fault was injected
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_7(void)
{
    faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_FRAMING_ERR, .firstByte = 34u, .count = 1u } };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_LIBOBD2_RequestService_7.vcd");
    APPFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_UartAttach(&appFx.uart, &inj));

    /* 2. */
    APPFX_QueueVehicleInfo(PID_09_VIN_MESSAGE_COUNT, PID_09_VIN, appFxVin, APPFX_VIN_MSG_CNT, APPFX_VIN_MSG_CNT);

    /* 3. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_VIN, response, &responseLen);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_FRAMING, status.response);
    TEST_ASSERT_EQUAL_size_t(0, responseLen);

    /* 4. */
    TEST_ASSERT_EQUAL_UINT32(1u, inj.injected);
}

/******************************************************************
 * - brief:       The ECU sends fewer VIN messages than announced
 * - description: The count answer promises 5 messages, only 3 come;
 *                the request fails waiting for the 4th instead of
 *                returning a short VIN.
 * - steps:
 *     1. ECU: count answer "5", then VIN messages 1..3 only (tester
 *        echo suppressed, see APPFX_SuppressEcho)
 *     2. RequestService returns OBD_ERR_COMM_FMT_BYTE_NOT_RECVD with
 *        the P2 max timeout
 *     3. No response data
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_8(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_LIBOBD2_RequestService_8.vcd");

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    APPFX_QueueVehicleInfo(PID_09_VIN_MESSAGE_COUNT, PID_09_VIN, appFxVin, APPFX_VIN_MSG_CNT, 3u);

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_VIN, response, &responseLen);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU, status.timeout);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
}

/******************************************************************
 * - brief:       The ECU never answers a live data request
 * - description: The coolant request goes out, nothing comes back:
 *                the request fails with the P2 max timeout and no data.
 * - steps:
 *     1. Empty line, tester echo suppressed (see APPFX_SuppressEcho)
 *     2. RequestService(coolant) returns OBD_ERR_COMM_FMT_BYTE_NOT_RECVD
 *        with OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU
 *     3. No response data
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_9(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_LIBOBD2_RequestService_9.vcd");

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU, status.timeout);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
}

/******************************************************************
 * - brief:       The ECU rejects the request (negative response)
 * - description: A 7F answer travels up to the caller as
 *                OBD_ERR_COMM_ECU_RESPONSE_7F; nothing is decoded.
 * - steps:
 *     1. Tester: 68 6A F1 01 05 CS        ECU: 48 6B 12 7F 01 12 CS
 *     2. RequestService returns OBD_ERR_COMM_ECU_RESPONSE_7F
 *     3. No response data
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_10(void)
{
    const uint8_t ecuResp[] = { 0x7Fu, SID_SHOW_CURRENT_DATA, 0x12u };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    /* 2. */
    UART_TRACE_Begin("test_neg_ex_LIBOBD2_RequestService_10.vcd");
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_ECU_RESPONSE_7F, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
}

/******************************************************************
 * - brief:       The ECU answers with a PID that was not asked for
 * - description: A positive $01 answer for an unknown PID (FF) cannot
 *                be decoded: OBD_NOT_SUPPORTED with no data, never a
 *                temperature computed from the wrong PID.
 * - steps:
 *     1. Tester: 68 6A F1 01 05 CS        ECU: 48 6B 12 41 FF 7B CS
 *     2. RequestService(coolant) returns OBD_NOT_SUPPORTED
 *     3. No response data
 ******************************************************************/
void test_neg_ex_LIBOBD2_RequestService_11(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, 0xFFu, 0x7Bu };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = NO_LEN;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    /* 2. */
    UART_TRACE_Begin("test_neg_ex_LIBOBD2_RequestService_11.vcd");
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
}
