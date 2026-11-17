/* ================================================ INCLUDES =============================================== */
#include "libobd2_services_neg_test.h"
#include "libobd2_fixture.h"
#include "iso15031_5.h"
#include "libobd2.h"
#include "libobd2_services.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <string.h>

/* ================================================= MACROS ================================================ */
#define NO_LEN ((size_t)0xDEADu)

/* ============================================ LOCAL VARIABLES ============================================ */
extern const libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES];
extern bool LibOBD2_IsAnswerTo(const libobd2_data_t *answer, uint8_t sid, uint8_t tid);
extern obd_status_t LibOBD2_DispatchSid(obd_ctx_t* ctx, const libobd2_service_t *libobd2Service, libobd2_data_t* response, size_t respCap, size_t * respLen);

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* Queues the given ECU frames back to back after a 1-byte request ($03 / $07 / $0A) */
static void queueAfterSidRequest(const uint8_t frames[][7], const size_t *lens, size_t frameCnt)
{
    uint32_t time = APPFX_REQ_DURATION(1u);

    for (size_t idx = 0; idx < frameCnt; idx++)
    {
        time = APPFX_QueueEcuFrame(time, frames[idx], lens[idx]);
    }
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       More answers than the buffer holds while reading
 *                until silence
 * - description: TP_READ_UNTIL_TO stops with OBD_ERR_APP_TOO_MANY_MSGS
 *                as soon as one answer more than respCap arrives,
 *                never writing past the buffer.
 * - steps:
 *     1. ECU: two $03 answers, no echo
 *     2. DispatchSid($03) with room for 1 message
 *     3. OBD_ERR_APP_TOO_MANY_MSGS, the first message kept
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_0(void)
{
    const uint8_t frames[][7] =
    {
        { SID_SHOW_STORED_DTCS + 0x40u, 0x01, 0x43, 0x00, 0x00, 0x00, 0x00 },
        { SID_SHOW_STORED_DTCS + 0x40u, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },
    };
    const size_t lens[] = { 7u, 7u };
    libobd2_data_t responseBuffer[2] = {0};
    size_t respLen = 0;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    queueAfterSidRequest(frames, lens, 2u);

    /* 2. */
    status = LibOBD2_DispatchSid(&appFx.ctx, &libobd2Services[OBD_GET_STORED_DTCS], responseBuffer, 1u, &respLen);

    /* 3. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_APP_TOO_MANY_MSGS, status.response);
    TEST_ASSERT_EQUAL_HEX8(frames[0][1], responseBuffer[0].data.param[0]);
    TEST_ASSERT_EQUAL_size_t(0u, responseBuffer[1].dataLen);
}

/******************************************************************
 * - brief:       A DTC answer without all 3 DTC slots is rejected
 * - description: Every $03 answer has 6 DTC bytes (ISO 15031-5 Table
 *                41). A shorter one fails the request with
 *                OBD_ERR_APP_SHORT_MSG and no data.
 * - steps:
 *     1. ECU: 48 6B 12 43 01 43 CS (one DTC only)
 *     2. RequestService(stored DTCs): OBD_ERR_APP_SHORT_MSG, len 0
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_1(void)
{
    const uint8_t frames[][7] = { { SID_SHOW_STORED_DTCS + 0x40u, 0x01, 0x43 } };
    const size_t lens[] = { 3u };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = NO_LEN;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    queueAfterSidRequest(frames, lens, 1u);

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_STORED_DTCS, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_APP_SHORT_MSG, status.response);
    TEST_ASSERT_EQUAL_size_t(0u, responseLen);
}

/******************************************************************
 * - brief:       An answer of another service among the DTC answers
 * - description: Every answer read until silence must be for the
 *                service asked; a $01 answer in between fails the
 *                request with no data.
 * - steps:
 *     1. ECU: 43 01 43 00 00 00 00, then 41 05 7B
 *     2. RequestService(stored DTCs): OBD_NOT_SUPPORTED, len 0
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_2(void)
{
    const uint8_t frames[][7] =
    {
        { SID_SHOW_STORED_DTCS + 0x40u, 0x01, 0x43, 0x00, 0x00, 0x00, 0x00 },
        { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_COOLANT_TEMP, 0x7B },
    };
    const size_t lens[] = { 7u, 3u };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = NO_LEN;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    queueAfterSidRequest(frames, lens, 2u);

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_STORED_DTCS, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, status.response);
    TEST_ASSERT_EQUAL_size_t(0u, responseLen);
}

/******************************************************************
 * - brief:       A 2-byte PID answer with only A is rejected
 * - description: RPM needs A and B. 41 0C 1A fails with
 *                OBD_ERR_APP_SHORT_MSG instead of decoding B as 0.
 * - steps:
 *     1. Tester: 68 6A F1 01 0C CS        ECU: 48 6B 12 41 0C 1A CS
 *     2. RequestService(RPM): OBD_ERR_APP_SHORT_MSG, len 0
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_3(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_ENGINE_RPM, 0x1Au };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = NO_LEN;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_ENGINE_RPM, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_APP_SHORT_MSG, status.response);
    TEST_ASSERT_EQUAL_size_t(0u, responseLen);
}

/******************************************************************
 * - brief:       An answer for a known PID that was not asked for
 * - description: Asked for RPM, the ECU answers vehicle speed: the
 *                library must not hand the speed back as the RPM.
 * - steps:
 *     1. Tester: 68 6A F1 01 0C CS        ECU: 48 6B 12 41 0D 32 CS
 *     2. RequestService(RPM): OBD_NOT_SUPPORTED, len 0
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_4(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_VEHICLE_SPEED, 0x32u };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = NO_LEN;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_ENGINE_RPM, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, status.response);
    TEST_ASSERT_EQUAL_size_t(0u, responseLen);
}

/******************************************************************
 * - brief:       A freeze frame answer for another frame
 * - description: Frame 0 is asked; an answer for frame 1 is not
 *                decoded.
 * - steps:
 *     1. Tester: 68 6A F1 02 05 00 CS     ECU: 48 6B 12 42 05 01 9A CS
 *     2. RequestService(freeze frame coolant): OBD_NOT_SUPPORTED, len 0
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_5(void)
{
    const uint8_t ecuResp[] = { SID_SHOW_FREEZE_FRAME_DATA + 0x40u, PID_01_COOLANT_TEMP, 0x01u, 0x9Au };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = NO_LEN;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(3u), ecuResp, sizeof(ecuResp));

    /* 2. */
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_FF_COOLANT_TEMP, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, status.response);
    TEST_ASSERT_EQUAL_size_t(0u, responseLen);
}

/******************************************************************
 * - brief:       Invalid arguments of the raw and test requests
 * - description: Each call rejects its arguments before anything goes
 *                on the line.
 * - steps:
 *     1. RequestRaw: NULL request -> NULL_PTR; length 0 or 13, no room
 *        for an answer -> INVALID_PARAM
 *     2. RequestSupportedTids: sid $01 or range $10 -> INVALID_PARAM
 *     3. O2 / monitor / control requests with a NULL result -> NULL_PTR
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_6(void)
{
    const obd_data_t req = { .sid = SID_SHOW_CURRENT_DATA, .param = { 0x00u } };
    const uint8_t dataIn[LIBOBD2_CONTROL_DATA_LEN] = {0};
    libobd2_data_t answers[2] = {0};
    libobd2_monitor_test_t results[2] = {0};
    size_t cnt = 0;
    uint32_t mask = 0;

    APPFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestRaw(&appFx.ctx, NULL, 2u, answers, 2u, &cnt).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, LibOBD2_RequestRaw(&appFx.ctx, &req, 0u, answers, 2u, &cnt).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, LibOBD2_RequestRaw(&appFx.ctx, &req, sizeof(obd_data_t) + 1u, answers, 2u, &cnt).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, LibOBD2_RequestRaw(&appFx.ctx, &req, 2u, answers, 0u, &cnt).response);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, LibOBD2_RequestSupportedTids(&appFx.ctx, SID_SHOW_CURRENT_DATA, 0x00u, 0u, &mask).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_INVALID_PARAM, LibOBD2_RequestSupportedTids(&appFx.ctx, SID_TEST_RESULTS_OTHER, 0x10u, 0u, &mask).response);

    /* 3. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestO2SensorTest(&appFx.ctx, 0x01u, 0x01u, NULL).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestMonitorTests(&appFx.ctx, 0x01u, NULL, 2u, &cnt).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestMonitorTests(&appFx.ctx, 0x01u, results, 2u, NULL).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestControl(&appFx.ctx, 0x01u, dataIn, NULL).response);
}

/******************************************************************
 * - brief:       More $06 results than the caller has room for
 * - description: The ECU reports 3 results for TID $01; with room for
 *                2 the request fails with OBD_ERR_APP_TOO_MANY_MSGS and
 *                no results.
 * - steps:
 *     1. 06 01 answered with 3 messages, room for 2
 *     2. OBD_ERR_APP_TOO_MANY_MSGS, resultCnt 0
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_7(void)
{
    const uint8_t frames[][7] =
    {
        { 0x46, 0x01, 0x01, 0x00, 0x64, 0x00, 0xC8 },
        { 0x46, 0x01, 0x81, 0x00, 0x64, 0x00, 0x32 },
        { 0x46, 0x01, 0x02, 0x01, 0x2C, 0x00, 0xC8 },
    };
    libobd2_monitor_test_t results[2] = {0};
    size_t resultCnt = NO_LEN;
    obd_status_t status = {0};
    uint32_t time = APPFX_REQ_DURATION(2u);

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    for (size_t idx = 0; idx < 3u; idx++)
    {
        time = APPFX_QueueEcuFrame(time, frames[idx], sizeof(frames[idx]));
    }
    status = LibOBD2_RequestMonitorTests(&appFx.ctx, 0x01u, results, 2u, &resultCnt);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_APP_TOO_MANY_MSGS, status.response);
    TEST_ASSERT_EQUAL_size_t(0u, resultCnt);
}

/******************************************************************
 * - brief:       A $05 answer for another O2 sensor or a short one
 * - description: Asked for sensor $01, an answer for sensor $02 is
 *                not taken; an answer without the test value fails
 *                with OBD_ERR_APP_SHORT_MSG.
 * - steps:
 *     1. 05 05 01 -> 45 05 02 19 0A 32: OBD_NOT_SUPPORTED
 *     2. 05 05 01 -> 45 05 01: OBD_ERR_APP_SHORT_MSG
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_8(void)
{
    const uint8_t otherSensor[] = { 0x45, 0x05, 0x02, 0x19, 0x0A, 0x32 };
    const uint8_t noValue[] = { 0x45, 0x05, 0x01 };
    libobd2_o2_test_t result = {0};
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(3u), otherSensor, sizeof(otherSensor));
    status = LibOBD2_RequestO2SensorTest(&appFx.ctx, 0x05u, 0x01u, &result);
    TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, status.response);

    /* 2. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(3u), noValue, sizeof(noValue));
    status = LibOBD2_RequestO2SensorTest(&appFx.ctx, 0x05u, 0x01u, &result);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_APP_SHORT_MSG, status.response);
}


/******************************************************************
 * - brief:       What counts as the answer to a $05 / $06 / $08 TID
 * - description: An answer must hold at least sid and TID, be the
 *                positive answer of the service asked and carry the
 *                TID asked.
 * - steps:
 *     1. 46 01 for 06 01: true
 *     2. 46 alone (no TID): false
 *     3. 45 01 for 06 01 (other service): false
 *     4. 46 02 for 06 01 (other TID): false
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_9(void)
{
    const libobd2_data_t ok      = { .data = { .sid = SID_TEST_RESULTS_OTHER + 0x40u, .param = { 0x01u } }, .dataLen = 2u };
    const libobd2_data_t noTid   = { .data = { .sid = SID_TEST_RESULTS_OTHER + 0x40u, .param = { 0x01u } }, .dataLen = 1u };
    const libobd2_data_t otherSid = { .data = { .sid = SID_TEST_RESULTS_O2_SENSORS + 0x40u, .param = { 0x01u } }, .dataLen = 2u };
    const libobd2_data_t otherTid = { .data = { .sid = SID_TEST_RESULTS_OTHER + 0x40u, .param = { 0x02u } }, .dataLen = 2u };

    /* 1. */
    TEST_ASSERT_TRUE(LibOBD2_IsAnswerTo(&ok, SID_TEST_RESULTS_OTHER, 0x01u));
    /* 2. */
    TEST_ASSERT_FALSE(LibOBD2_IsAnswerTo(&noTid, SID_TEST_RESULTS_OTHER, 0x01u));
    /* 3. */
    TEST_ASSERT_FALSE(LibOBD2_IsAnswerTo(&otherSid, SID_TEST_RESULTS_OTHER, 0x01u));
    /* 4. */
    TEST_ASSERT_FALSE(LibOBD2_IsAnswerTo(&otherTid, SID_TEST_RESULTS_OTHER, 0x01u));
}

/******************************************************************
 * - brief:       NULL context, data link or buffers
 * - description: The raw and test requests reject a NULL context, a
 *                context without a data link and every NULL buffer
 *                with OBD_ERR_NULL_PTR, before anything goes on the line.
 * - steps:
 *     1. NULL ctx for each request
 *     2. a ctx with no data link for each request
 *     3. the remaining NULL buffers (raw answers / count, supported
 *        mask, control data in)
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_10(void)
{
    const obd_data_t req = { .sid = SID_SHOW_CURRENT_DATA, .param = { 0x00u } };
    const uint8_t dataIn[LIBOBD2_CONTROL_DATA_LEN] = {0};
    uint8_t dataOut[LIBOBD2_CONTROL_DATA_LEN] = {0};
    obd_ctx_t noLink = { .pDataLink = NULL };
    obd_ctx_t *ctxs[2] = { NULL, &noLink };
    libobd2_data_t answers[2] = {0};
    libobd2_monitor_test_t results[2] = {0};
    libobd2_o2_test_t o2 = {0};
    size_t cnt = 0;
    uint32_t mask = 0;

    APPFX_SetUpLine();

    /* 1. - 2. */
    for (size_t idx = 0; idx < 2u; idx++)
    {
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestRaw(ctxs[idx], &req, 2u, answers, 2u, &cnt).response);
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestSupportedTids(ctxs[idx], SID_TEST_RESULTS_OTHER, 0x00u, 0u, &mask).response);
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestO2SensorTest(ctxs[idx], 0x01u, 0x01u, &o2).response);
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestMonitorTests(ctxs[idx], 0x01u, results, 2u, &cnt).response);
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestControl(ctxs[idx], 0x01u, dataIn, dataOut).response);
    }

    /* 3. */
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestRaw(&appFx.ctx, &req, 2u, NULL, 2u, &cnt).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestRaw(&appFx.ctx, &req, 2u, answers, 2u, NULL).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestSupportedTids(&appFx.ctx, SID_TEST_RESULTS_OTHER, 0x00u, 0u, NULL).response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_NULL_PTR, LibOBD2_RequestControl(&appFx.ctx, 0x01u, NULL, dataOut).response);
}

/******************************************************************
 * - brief:       The ECU does not answer a $05 / $06 / $08 request
 * - description: Requests that expect exactly one answer fail with the
 *                receive error when none comes, and leave the outputs
 *                alone.
 * - steps:
 *     1. No echo, no ECU answer
 *     2. RequestSupportedTids for $05, $06, $08: OBD_ERR_COMM_FMT_BYTE_NOT_RECVD
 *     3. RequestO2SensorTest, RequestControl: same error
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_11(void)
{
    const uint8_t sids[] = { SID_TEST_RESULTS_O2_SENSORS, SID_TEST_RESULTS_OTHER, SID_CONTROL_ONBOARD_SYSTEM };
    const uint8_t dataIn[LIBOBD2_CONTROL_DATA_LEN] = {0};
    uint8_t dataOut[LIBOBD2_CONTROL_DATA_LEN] = {0};
    libobd2_o2_test_t o2 = {0};
    uint32_t mask = 0;

    for (size_t idx = 0; idx < sizeof(sids); idx++)
    {
        /* 1. */
        APPFX_SetUpLine();
        APPFX_SuppressEcho();

        /* 2. */
        mask = 0xA5A5A5A5u;
        TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD,
                                LibOBD2_RequestSupportedTids(&appFx.ctx, sids[idx], 0x00u, 0x01u, &mask).response);
        TEST_ASSERT_EQUAL_HEX32(0xA5A5A5A5u, mask);
    }

    /* 3. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, LibOBD2_RequestO2SensorTest(&appFx.ctx, 0x01u, 0x01u, &o2).response);
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, LibOBD2_RequestControl(&appFx.ctx, 0x01u, dataIn, dataOut).response);
}

/******************************************************************
 * - brief:       Supported-TIDs answers that do not fit the request
 * - description: The answer to 05 00 01 must be 45 00 01 A B C D.
 *                Another range or O2 sensor is not taken; a shorter
 *                answer fails with OBD_ERR_APP_SHORT_MSG. The mask is
 *                left alone in every case.
 * - steps:
 *     1. 45 20 01 80 00 00 00 (range $20): OBD_NOT_SUPPORTED
 *     2. 45 00 02 80 00 00 00 (sensor $02): OBD_NOT_SUPPORTED
 *     3. 45 00 01 80 (no B C D): OBD_ERR_APP_SHORT_MSG
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_12(void)
{
    const uint8_t otherRange[]  = { 0x45u, 0x20u, 0x01u, 0x80u, 0x00u, 0x00u, 0x00u };
    const uint8_t otherSensor[] = { 0x45u, 0x00u, 0x02u, 0x80u, 0x00u, 0x00u, 0x00u };
    const uint8_t shortMask[]   = { 0x45u, 0x00u, 0x01u, 0x80u };
    const uint8_t *answers[] = { otherRange, otherSensor, shortMask };
    const size_t lens[] = { sizeof(otherRange), sizeof(otherSensor), sizeof(shortMask) };
    const uint16_t expected[] = { OBD_NOT_SUPPORTED, OBD_NOT_SUPPORTED, OBD_ERR_APP_SHORT_MSG };
    uint32_t mask = 0;

    for (size_t idx = 0; idx < 3u; idx++)
    {
        mask = 0xA5A5A5A5u;
        APPFX_SetUpLine();
        (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(3u), answers[idx], lens[idx]);
        TEST_ASSERT_EQUAL_HEX16(expected[idx],
                                LibOBD2_RequestSupportedTids(&appFx.ctx, SID_TEST_RESULTS_O2_SENSORS, 0x00u, 0x01u, &mask).response);
        TEST_ASSERT_EQUAL_HEX32(0xA5A5A5A5u, mask);
    }
}

/******************************************************************
 * - brief:       $06 answers that do not fit the request
 * - description: Every answer to 06 01 must be 46 01 TLTCID value limit
 *                (7 bytes). One for another TID, one of another length
 *                or an answer cut off before its checksum fail the
 *                request with no results.
 * - steps:
 *     1. 46 01 .., then 46 02 ..: OBD_NOT_SUPPORTED
 *     2. 46 01 01 00 64 (no limit): OBD_ERR_APP_SHORT_MSG
 *     3. 48 6B 12 46 01, then nothing (no checksum): the receive
 *        error, not the end of the answers
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_13(void)
{
    const uint8_t good[]     = { 0x46u, 0x01u, 0x01u, 0x00u, 0x64u, 0x00u, 0xC8u };
    const uint8_t otherTid[] = { 0x46u, 0x02u, 0x01u, 0x00u, 0x64u, 0x00u, 0xC8u };
    const uint8_t noLimit[]  = { 0x46u, 0x01u, 0x01u, 0x00u, 0x64u };
    const uint8_t cutFrame[] = { 0x48u, 0x6Bu, 0x12u, 0x46u, 0x01u };
    libobd2_monitor_test_t results[32] = {0};
    size_t resultCnt = NO_LEN;
    uint32_t time = 0u;

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    time = APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), good, sizeof(good));
    (void)APPFX_QueueEcuFrame(time, otherTid, sizeof(otherTid));
    TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, LibOBD2_RequestMonitorTests(&appFx.ctx, 0x01u, results, 32u, &resultCnt).response);
    TEST_ASSERT_EQUAL_size_t(0u, resultCnt);

    /* 2. */
    resultCnt = NO_LEN;
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), noLimit, sizeof(noLimit));
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_APP_SHORT_MSG, LibOBD2_RequestMonitorTests(&appFx.ctx, 0x01u, results, 32u, &resultCnt).response);
    TEST_ASSERT_EQUAL_size_t(0u, resultCnt);

    /* 3. */
    resultCnt = NO_LEN;
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    time = APPFX_REQ_DURATION(2u) + P2_TIME_MIN;
    for (size_t idx = 0; idx < sizeof(cutFrame); idx++)
    {
        appFx.timer.timeNow = time + ((uint32_t)idx * APPFX_ECU_BYTE_GAP);
        UART_WriteByte(&appFx.uart, cutFrame[idx]);
    }
    appFx.timer.timeNow = 0u;
    TEST_ASSERT_NOT_EQUAL_HEX16(OBD_STATUS_OK, LibOBD2_RequestMonitorTests(&appFx.ctx, 0x01u, results, 32u, &resultCnt).response);
    TEST_ASSERT_EQUAL_size_t(0u, resultCnt);
}

/******************************************************************
 * - brief:       $08 answers that do not fit the request
 * - description: The answer to 08 01 .. must be 48 01 A B C D E. One
 *                for another TID is not taken, a shorter one fails
 *                with OBD_ERR_APP_SHORT_MSG; dataOut is left alone.
 * - steps:
 *     1. 48 02 00 00 00 00 00: OBD_NOT_SUPPORTED
 *     2. 48 01 00: OBD_ERR_APP_SHORT_MSG
 ******************************************************************/
void test_neg_ex_LIBOBD2_Services_14(void)
{
    const uint8_t otherTid[] = { 0x48u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    const uint8_t shortAns[] = { 0x48u, 0x01u, 0x00u };
    const uint8_t untouched[LIBOBD2_CONTROL_DATA_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    const uint8_t dataIn[LIBOBD2_CONTROL_DATA_LEN] = {0};
    uint8_t dataOut[LIBOBD2_CONTROL_DATA_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(7u), otherTid, sizeof(otherTid));
    TEST_ASSERT_EQUAL_HEX16(OBD_NOT_SUPPORTED, LibOBD2_RequestControl(&appFx.ctx, 0x01u, dataIn, dataOut).response);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(untouched, dataOut, LIBOBD2_CONTROL_DATA_LEN);

    /* 2. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(7u), shortAns, sizeof(shortAns));
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_APP_SHORT_MSG, LibOBD2_RequestControl(&appFx.ctx, 0x01u, dataIn, dataOut).response);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(untouched, dataOut, LIBOBD2_CONTROL_DATA_LEN);
}
