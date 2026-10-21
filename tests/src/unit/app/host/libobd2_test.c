/* ================================================ INCLUDES =============================================== */
#include "libobd2_test.h"
#include "datalink.h"
#include "iso15031_5.h"
#include "l2_iso9141.h"
#include "libobd2.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <string.h>
/* ================================================= MACROS ================================================ */
#define TIME_STEP          10u /* virtual ms consumed by every UART poll of the tester */
#define ECU_BYTE_GAP       5u  /* ECU inter-byte time (P1), kept <= TIME_STEP so every byte is ready when polled */
#define ISO9141_HDR_LEN    3u
#define RESPONSE_BUF_LEN   64u
#define VIN_LEN            17u
#define VIN_MSG_CNT        5u  /* 17 VIN chars + 3 pad bytes, 4 data bytes per message */
#define CALID_LEN          16u
#define CALID_MSG_CNT      4u

/* Time the tester needs to send a request of `dataLen` bytes (sid + params): P3 idle, then P4 after each byte */
#define REQ_DURATION(dataLen) (P3_TIME_MIN + (ISO9141_HDR_LEN + (dataLen) + 1u) * P4_TIME_MIN)

/* ============================================ LOCAL VARIABLES ============================================ */
static virtTmrCtx_t timer;
static virtUartLinkCtx_t uartCtx;
static l2_iso9141_ctx_t protoCtx;
static dataLink_if_t dataLink;
static obd_ctx_t ctx;

static const header_t header_ecu =
{
    .fmt     = 0x48,
    .trgAddr = 0x6B,
    .srcAddr = 0x12,
};

static const header_t header_tester =
{
    .fmt     = 0x68,
    .trgAddr = 0x6A,
    .srcAddr = 0xF1,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen);

static bool keepTimer(void *pHandle);
static bool keepLine(void *handle);

/* DL_Connect() re-initialises the timer and the transport. On the virtual line that would zero timeStep and
 * drop the ECU bytes the test queued, so the test owns both and these inits keep them as they are. */
static obd_timing_ops_t tmrOps =
{
    .timer_init         = keepTimer,
    .delay_ms           = LIBOBD2_TMR_DelayMs,
    .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
    .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
    .start_timeout      = LIBOBD2_TMR_StartTimeout,
    .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

static obd_uart_ops_t uartOps =
{
    .init        = keepLine,
    .send_byte   = LIBOBD2_UART_WriteByte,
    .recv_byte   = LIBOBD2_UART_RecvByte,
    .send_pulse  = LIBOBD2_UART_SendPulse,
    .switch_mode = LIBOBD2_UART_SwitchMode,
    .flush_rx    = LIBOBD2_UART_FlushRx,
};

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static bool keepTimer(void *pHandle)
{
    (void)pHandle;
    return true;
}

static bool keepLine(void *handle)
{
    (void)handle;
    return true;
}

/******************************************************************
 * Fresh virtual K-line with the tester already connected over
 * ISO9141 (header 68 6A F1), clock at 0 and an empty line.
 ******************************************************************/
static void setUpLine(void)
{
    timer = (virtTmrCtx_t){ .timeStep = TIME_STEP };
    uartCtx.tmrHandle       = &timer;
    uartCtx.erorr_generator = NULL;
    TEST_ASSERT_TRUE(LIBOBD2_UART_Init(&uartCtx));

    protoCtx = (l2_iso9141_ctx_t){ .header = header_tester, .kb1 = 0x08u, .kb2 = 0x08u };
    tmrOps.timeSample = 0u;

    dataLink = (dataLink_if_t)
    {
        .pProtocolCtx     = &protoCtx,
        .pTimingOps       = &tmrOps,
        .pTimingHandle    = &timer,
        .pUartOps         = &uartOps,
        .pTransportHandle = &uartCtx,
        .connect          = l2_iso9141_connect,
        .send_request     = l2_iso9141_send_request,
        .recv_response    = l2_iso9141_recv_response,
    };

    ctx = (obd_ctx_t){ .pDataLink = &dataLink, .connectionStatus = 0 };
}

/******************************************************************
 * Queue one ECU response frame (48 6B 12 <data> <cs>) on the line.
 *   - reqEnd: virtual time at which the tester stopped talking
 *   - the format byte goes out P2 min after reqEnd, the rest
 *     ECU_BYTE_GAP apart
 * Returns the virtual time at which the tester has read the whole
 * frame: it polls every TIME_STEP from reqEnd on, and every read
 * costs one TIME_STEP.
 ******************************************************************/
static uint32_t ecuQueueFrame(uint32_t reqEnd, const uint8_t *data, size_t dataLen)
{
    uint8_t frame[ISO9141_HDR_LEN + sizeof(obd_data_t) + 1u] = {0};
    obd_data_t obdData = {0};
    size_t frameLen = 0;
    uint32_t firstByteTime = reqEnd + P2_TIME_MIN;

    memcpy(&obdData, data, dataLen);
    frame[frameLen++] = header_ecu.fmt;
    frame[frameLen++] = header_ecu.trgAddr;
    frame[frameLen++] = header_ecu.srcAddr;
    memcpy(&frame[frameLen], data, dataLen);
    frameLen += dataLen;
    frame[frameLen++] = L2_ISO9141_ComputeChecksum(header_ecu, obdData, dataLen);

    for (size_t idx = 0; idx < frameLen; idx++)
    {
        timer.timeNow = firstByteTime + (uint32_t)idx * ECU_BYTE_GAP;
        LIBOBD2_UART_WriteByte(&uartCtx, frame[idx]);
    }
    timer.timeNow = 0u;

    /* first poll that sees the format byte, then one poll per byte */
    uint32_t firstPoll = reqEnd + ((P2_TIME_MIN / TIME_STEP) + 1u) * TIME_STEP;
    return firstPoll + (uint32_t)frameLen * TIME_STEP;
}

/******************************************************************
 * Queue the ECU side of a SID $09 multi-message exchange:
 *   1. message count response   49 <cntPid> <msgCnt>
 *   2. msgCnt data messages     49 <pid> <msgNr> D1 D2 D3 D4
 * `payload` holds the msgCnt * 4 data bytes (D1..D4 of every message)
 ******************************************************************/
static void ecuQueueVehicleInfo(uint8_t cntPid, uint8_t pid, const uint8_t *payload, uint8_t msgCnt)
{
    const uint8_t sid = SID_REQUEST_VEHICLE_INFO + 0x40u;
    const uint8_t cntResp[] = { sid, cntPid, msgCnt };
    uint32_t time = 0u;

    /* tester asks for the message count */
    time += REQ_DURATION(2u);
    time  = ecuQueueFrame(time, cntResp, sizeof(cntResp));

    /* tester asks for the data, the ECU answers with every message back to back */
    time += REQ_DURATION(2u);
    for (uint8_t msgNr = 1u; msgNr <= msgCnt; msgNr++)
    {
        const uint8_t *d = &payload[(msgNr - 1u) * 4u];
        const uint8_t msg[] = { sid, pid, msgNr, d[0], d[1], d[2], d[3] };
        time = ecuQueueFrame(time, msg, sizeof(msg));
    }
}

/* ================================================ MODULE API ============================================= */
void test_LIBOBD2_Init_NullCtx(void)
{
    obd_status_t status = LibOBD2_Init(NULL);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, status.response);
}

void test_LIBOBD2_Init_NullDataLink(void)
{
    obd_ctx_t nullLinkCtx = { .pDataLink = NULL };

    obd_status_t status = LibOBD2_Init(&nullLinkCtx);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, status.response);
}

/******************************************************************
 * Connect to an ISO9141-2 ECU on the virtual K-line.
 *   1. Tester idles W5, then sends 0x33 at 5 baud
 *   2. ECU answers sync 0x55, KB1 0x08, KB2 0x08 and, after the
 *      tester's ~KB2, the inverted address 0xCC
 *   3. LibOBD2_Init must report OK and bind the datalink to ISO9141
 *      with the 68 6A F1 header
 ******************************************************************/
void test_LIBOBD2_Init_000(void)
{
    setUpLine();
    /* nothing is known about the protocol before the connect */
    protoCtx = (l2_iso9141_ctx_t){0};
    dataLink.connect       = NULL;
    dataLink.send_request  = NULL;
    dataLink.recv_response = NULL;

    /* Same ECU timing as test_ISO9141_5BaudInit_000: W5 + 10 bits at 5 baud, then W1..W4 */
    timer.timeNow = ISO9141_W5_TIME_MIN + 2000u + ISO9141_W1_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x55u);
    timer.timeNow += ISO9141_W2_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x08u);
    timer.timeNow += ISO9141_W3_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, 0x08u);
    timer.timeNow += 3u * ISO9141_W4_TIME_MIN;
    LIBOBD2_UART_WriteByte(&uartCtx, (uint8_t)(~0x33u));
    timer.timeNow = 0u;

    UART_TRACE_Begin("test_LIBOBD2_Init_000.vcd");
    obd_status_t status = LibOBD2_Init(&ctx);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_PTR(l2_iso9141_connect, dataLink.connect);
    TEST_ASSERT_EQUAL_PTR(l2_iso9141_send_request, dataLink.send_request);
    TEST_ASSERT_EQUAL_PTR(l2_iso9141_recv_response, dataLink.recv_response);
    TEST_ASSERT_EQUAL_HEX8(header_tester.fmt, protoCtx.header.fmt);
    TEST_ASSERT_EQUAL_HEX8(header_tester.trgAddr, protoCtx.header.trgAddr);
    TEST_ASSERT_EQUAL_HEX8(header_tester.srcAddr, protoCtx.header.srcAddr);
    TEST_ASSERT_EQUAL_HEX8(0x08u, protoCtx.kb1);
    TEST_ASSERT_EQUAL_HEX8(0x08u, protoCtx.kb2);
}

void test_LIBOBD2_RequestService_NullResponse(void)
{
    size_t responseLen = 0;

    setUpLine();

    obd_status_t status = LibOBD2_RequestService(&ctx, OBD_GET_VIN, NULL, &responseLen);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, status.response);
}

void test_LIBOBD2_RequestService_NullResponseLen(void)
{
    uint8_t response[RESPONSE_BUF_LEN] = {0};

    setUpLine();

    obd_status_t status = LibOBD2_RequestService(&ctx, OBD_GET_VIN, response, NULL);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, status.response);
}

/******************************************************************
 * An unknown service is rejected before anything goes on the line.
 ******************************************************************/
void test_LIBOBD2_RequestService_InvalidSid(void)
{
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;

    setUpLine();

    obd_status_t status = LibOBD2_RequestService(&ctx, OBD2_MAX_NUM_SERVICES, response, &responseLen);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, status.response);
    TEST_ASSERT_EQUAL_size_t(0, responseLen);
    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeNow);
}

/******************************************************************
 * Read the VIN (SID $09) over the virtual K-line.
 *   1. Tester: 68 6A F1 09 01 CS        ECU: 48 6B 12 49 01 05 CS
 *   2. Tester: 68 6A F1 09 02 CS        ECU: 5 x 48 6B 12 49 02 nn D1..D4 CS
 *   3. The response is D1..D4 of every message, in order:
 *      3 pad bytes followed by the 17 VIN characters
 ******************************************************************/
void test_LIBOBD2_RequestService_VIN_000(void)
{
    const uint8_t vin[VIN_MSG_CNT * 4u] = { 0x00, 0x00, 0x00, '1', 'H', 'G', 'C', 'M', '8', '2',
                                            '6', '3', '3', 'A', '0', '0', '4', '3', '5', '2' };
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;

    setUpLine();
    ecuQueueVehicleInfo(PID_09_VIN_MESSAGE_COUNT, PID_09_VIN, vin, VIN_MSG_CNT);

    UART_TRACE_Begin("test_LIBOBD2_RequestService_VIN_000.vcd");
    obd_status_t status = LibOBD2_RequestService(&ctx, OBD_GET_VIN, response, &responseLen);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(sizeof(vin), responseLen);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(vin, response, sizeof(vin));
    TEST_ASSERT_EQUAL_CHAR_ARRAY("1HGCM82633A004352", &response[sizeof(vin) - VIN_LEN], VIN_LEN);
}

/******************************************************************
 * Read the calibration ID (SID $09) over the virtual K-line.
 *   1. Tester: 68 6A F1 09 03 CS        ECU: 48 6B 12 49 03 04 CS
 *   2. Tester: 68 6A F1 09 04 CS        ECU: 4 x 48 6B 12 49 04 nn D1..D4 CS
 *   3. The response is the 16 calibration ID bytes, in order
 ******************************************************************/
void test_LIBOBD2_RequestService_CALID_000(void)
{
    const uint8_t calId[CALID_LEN] = { 'J', 'M', 'B', '*', '3', '6', '7', '6',
                                       '1', '5', '0', '0', 0x00, 0x00, 0x00, 0x00 };
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;

    setUpLine();
    ecuQueueVehicleInfo(PID_09_CALID_MESSAGE_COUNT, PID_09_CALID, calId, CALID_MSG_CNT);

    UART_TRACE_Begin("test_LIBOBD2_RequestService_CALID_000.vcd");
    obd_status_t status = LibOBD2_RequestService(&ctx, OBD_GET_CALID, response, &responseLen);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(CALID_LEN, responseLen);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(calId, response, CALID_LEN);
}

/******************************************************************
 * Read the engine coolant temperature (SID $01 PID $05).
 *   1. Tester: 68 6A F1 01 05 CS        ECU: 48 6B 12 41 05 7B CS
 *   2. The response is one byte, A - 40: 0x7B (123) -> 83 degC
 ******************************************************************/
void test_LIBOBD2_RequestService_CoolantTemp_000(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_COOLANT_TEMP, 0x7BU };
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;

    setUpLine();
    (void)ecuQueueFrame(REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    UART_TRACE_Begin("test_LIBOBD2_RequestService_CoolantTemp_000.vcd");
    obd_status_t status = LibOBD2_RequestService(&ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    // TEST_ASSERT_EQUAL_size_t(1, responseLen);
    TEST_ASSERT_EQUAL_UINT8(83u, response[0]);
}
