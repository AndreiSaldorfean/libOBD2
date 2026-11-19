/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "datalink.h"
#include "iso15031_5.h"
#include "libobd2_formulas.h"
#include "libobd2_service_requests.h"
#include "libobd2_services.h"
#include "libobd2_transfer_protocol.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <stddef.h>

/* ================================================= MACROS ================================================ */
#define LIBOBD2_POSITIVE_RESP_OFFSET  (0x40U) /* sid of a positive response = request sid + 0x40 */
#define LIBOBD2_RESP_BUF_MSGS         (20U)   /* ECU messages one request can collect */
#define LIBOBD2_PID_IDX               (0U)    /* param[] index of the PID */
#define LIBOBD2_MSG_CNT_IDX           (1U)    /* param[] index of the message count in a $09 count response */
#define LIBOBD2_LIVE_DATA_IDX         (1U)    /* param[] index of A in a $01 answer: PID A.. */
#define LIBOBD2_FREEZE_FRAME_IDX      (2U)    /* param[] index of A in a $02 answer: PID frame A.. */
#define LIBOBD2_FRAME_NR_IDX          (1U)    /* param[] index of the frame number in $02 */
#define LIBOBD2_DTCS_PER_MSG          (3U)    /* DTCs in one $03/$07/$0A answer */
#define LIBOBD2_DTC_MSG_LEN           (1U + (2U * LIBOBD2_DTCS_PER_MSG)) /* sid + 3 x 2 DTC bytes */
#define LIBOBD2_TID_RANGE             (0x20U) /* TIDs per supported-TIDs answer */
#define LIBOBD2_SUPPORTED_TIDS_LEN    (7U)    /* sid TID O2S/filler A B C D */
#define LIBOBD2_SUPPORTED_TIDS_IDX    (2U)    /* param[] index of A in a supported-TIDs answer */
#define LIBOBD2_O2_TEST_LEN           (4U)    /* 45 TID O2S value */
#define LIBOBD2_O2_TEST_LIMITS_LEN    (6U)    /* 45 TID O2S value min max */
#define LIBOBD2_MONITOR_TEST_LEN      (7U)    /* 46 TID TLTCID value(2) limit(2) */
#define LIBOBD2_MIN_LIMIT_MASK        (0x80U) /* $06 TLTCID bit 7: limit is a minimum */
#define LIBOBD2_COMPONENT_MASK        (0x7FU)
#define LIBOBD2_CONTROL_LEN           (1U + 1U + LIBOBD2_CONTROL_DATA_LEN) /* 08/48 TID A..E */

/* ============================================ LOCAL VARIABLES ============================================ */
OBD2_STATIC const libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES] = {OBD_SUPPORTED_SERVICES};
OBD2_STATIC const libobd2_pid_formula_t libobd2PidFormulas[] = {LIBOBD2_PID_FORMULAS};
OBD2_STATIC const size_t libobd2PidFormulaCnt = sizeof(libobd2PidFormulas) / sizeof(libobd2PidFormulas[0]);

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC obd_status_t LibOBD2_DispatchSid(obd_ctx_t* ctx, const libobd2_service_t *libobd2Service, libobd2_data_t* response, size_t respCap, size_t * respLen);
OBD2_STATIC bool LibOBD2_IsSilentP2(obd_status_t status);
OBD2_STATIC bool LibOBD2_IsDtcSid(uint8_t sid);
OBD2_STATIC const libobd2_data_t *LibOBD2_LastRequest(const libobd2_service_t *libobd2Service);
OBD2_STATIC obd_status_t LibOBD2_DecodePid(const libobd2_data_t *msg, size_t dataIdx, uint8_t *response, size_t* responseLen);
OBD2_STATIC obd_status_t LibOBD2_DecodeFreezeFrame(const libobd2_data_t *request, const libobd2_data_t *msg, uint8_t *response, size_t* responseLen);
OBD2_STATIC obd_status_t LibOBD2_DecodeDtcs(const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen);
OBD2_STATIC obd_status_t LibOBD2_DecodeVehicleInfo(const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen);
OBD2_STATIC obd_status_t LibOBD2_DecodeResponse(const libobd2_data_t *request, const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen);

OBD2_STATIC obd_status_t LibOBD2_Exchange(obd_ctx_t *ctx, const obd_data_t *request, size_t requestLen, bool untilSilent, libobd2_data_t *answers, size_t answerCap, size_t *answerCnt);
OBD2_STATIC bool LibOBD2_IsAnswerTo(const libobd2_data_t *answer, uint8_t sid, uint8_t tid);
OBD2_STATIC void LibOBD2_O2TestScaling(uint8_t tid, float *scale, libobd2_unit_t *unit);


OBD2_STATIC obd_status_t LibOBD2_RequestService_Validate(obd_ctx_t *ctx, libobd2_sid_t obdSid, uint8_t *response, size_t* responseLen);
OBD2_STATIC obd_status_t LibOBD2_RequestService_Execute(obd_ctx_t *ctx, libobd2_sid_t obdSid, uint8_t *response, size_t* responseLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: LibOBD2_IsSilentP2
* brief: True when DL_RecvResponse saw no answer start within P2 max: the
*        ECU(s) have nothing more to send.
******************************************/
OBD2_STATIC bool LibOBD2_IsSilentP2(obd_status_t status)
{
    return (OBD_ERR_COMM_FMT_BYTE_NOT_RECVD == status.response) &&
           (OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU == status.timeout);
}

/******************************************
* title: LibOBD2_IsDtcSid
* brief: True for the services that report DTCs ($03, $07, $0A).
******************************************/
OBD2_STATIC bool LibOBD2_IsDtcSid(uint8_t sid)
{
    return (SID_SHOW_STORED_DTCS == sid) || (SID_SHOW_PENDING_DTCS == sid) || (SID_PERMANENT_DTCS == sid);
}

/******************************************
* title: LibOBD2_LastRequest
* brief: The last request of a service: the one the collected answers reply to.
******************************************/
OBD2_STATIC const libobd2_data_t *LibOBD2_LastRequest(const libobd2_service_t *libobd2Service)
{
    size_t idx = 0;

    OBD2_DEBUG_ASSERT(NULL != libobd2Service);

    while (((idx + 1U) < LIBOBD2_MAX_SREQ) && (0U < libobd2Service->serviceRequests[idx + 1U].dataLen))
    {
        idx++;
    }

    return &libobd2Service->serviceRequests[idx];
}

/******************************************
* title: LibOBD2_DispatchSid
* brief: Sends the service's requests and collects the ECU messages into
*        response (at most respCap), following its transfer protocol.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_DispatchSid(obd_ctx_t* ctx, const libobd2_service_t *libobd2Service, libobd2_data_t* response, size_t respCap, size_t * respLen)
{
    dataLink_if_t *pDataLink = NULL;
    const libobd2_data_t *pServiceRequests = NULL;
    const uint8_t *tp = NULL;
    obd_status_t status = {0};
    obd_data_t resp = {0};
    uint8_t tpIdx = 0;
    uint8_t tpState = TP_END;
    size_t respIdx = 0;
    size_t msgCnt = 0;
    size_t msgIdx = 0;
    size_t len = 0;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->pDataLink) && (NULL != libobd2Service));
    OBD2_DEBUG_ASSERT((NULL != response) && (0U < respCap) && (NULL != respLen));

    pDataLink        = ctx->pDataLink;
    pServiceRequests = libobd2Service->serviceRequests;
    tp               = libobd2Service->transferProtocol;
    tpState          = tp[tpIdx];

    if (OBD_SHORT_SID == libobd2Service->sidSize)
    {
        status = DL_SendRequest(pDataLink, &pServiceRequests[0].data, pServiceRequests[0].dataLen);
        OBD2_ASSERT_OK(status);
        status = DL_RecvResponse(pDataLink, &resp, &len);
        OBD2_ASSERT_OK(status);

        response[0].data    = resp;
        response[0].dataLen = len;
        *respLen = 1;
        goto exit;
    }

    while (TP_END != tpState)
    {
        switch (tpState)
        {
            case TP_SEND:
                status = DL_SendRequest(pDataLink, &pServiceRequests[msgIdx].data, pServiceRequests[msgIdx].dataLen);
                OBD2_ASSERT_OK(status);
                msgIdx++;
                break;

            case TP_RECV:
                status = DL_RecvResponse(pDataLink, &resp, &len);
                OBD2_ASSERT_OK(status);
                response[respIdx].data      = resp;
                response[respIdx++].dataLen = len;
                break;

            case TP_MSG_CNT:
                status = DL_RecvResponse(pDataLink, &resp, &len);
                OBD2_ASSERT_OK(status);
                msgCnt = resp.param[LIBOBD2_MSG_CNT_IDX];
                break;

            case TP_MSGS:
                // The ECU announced the count; refuse more messages than the buffer holds
                status.response = OBD_ERR_APP_TOO_MANY_MSGS;
                OBD2_IF_COND_GOTO_EXIT(msgCnt > (respCap - respIdx));
                status.response = OBD_STATUS_OK;

                for (size_t i = 0; i < msgCnt; i++)
                {
                    status = DL_RecvResponse(pDataLink, &resp, &len);
                    OBD2_ASSERT_OK(status);
                    response[respIdx].data      = resp;
                    response[respIdx++].dataLen = len;
                }
                break;

            case TP_READ_UNTIL_TO:
                // Each answer starts within P2 of the previous one; a silent P2 window ends the read
                while (true)
                {
                    status = DL_RecvResponse(pDataLink, &resp, &len);
                    if (LibOBD2_IsSilentP2(status))
                    {
                        status = (obd_status_t){0};
                        break;
                    }
                    OBD2_ASSERT_OK(status);

                    status.response = OBD_ERR_APP_TOO_MANY_MSGS;
                    OBD2_IF_COND_GOTO_EXIT(respIdx >= respCap);
                    status.response = OBD_STATUS_OK;

                    response[respIdx].data      = resp;
                    response[respIdx++].dataLen = len;
                }
                break;

            default:
                break;
        }

        tpState = tp[++tpIdx];
    }
    *respLen = respIdx;

exit:
    return status;
}

/******************************************
* title: LibOBD2_DecodePid
* brief: Decodes the PID of a $01/$02 answer with its formula. dataIdx is the
*        param[] index of data byte A. Answers too short for the PID fail.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_DecodePid(const libobd2_data_t *msg, size_t dataIdx, uint8_t *response, size_t* responseLen)
{
    const libobd2_pid_formula_t *pidFormula = NULL;
    obd_status_t status = {0};
    uint8_t pid = 0;

    OBD2_DEBUG_ASSERT((NULL != msg) && (NULL != response) && (NULL != responseLen));

    pid = msg->data.param[LIBOBD2_PID_IDX];

    for (size_t idx = 0; idx < libobd2PidFormulaCnt; idx++)
    {
        if (pid == libobd2PidFormulas[idx].pid)
        {
            pidFormula = &libobd2PidFormulas[idx];
            break;
        }
    }

    status.response = OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT(NULL == pidFormula);

    // sid + params up to and including the last data byte the formula reads
    status.response = OBD_ERR_APP_SHORT_MSG;
    OBD2_IF_COND_GOTO_EXIT(msg->dataLen < (1U + dataIdx + pidFormula->dataLen));
    status.response = OBD_STATUS_OK;

    *responseLen = pidFormula->formula(&msg->data.param[dataIdx], response);

exit:
    return status;
}

/******************************************
* title: LibOBD2_DecodeFreezeFrame
* brief: Decodes a $02 answer: PID frame A.., for the frame that was asked.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_DecodeFreezeFrame(const libobd2_data_t *request, const libobd2_data_t *msg, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};

    OBD2_DEBUG_ASSERT((NULL != request) && (NULL != msg));

    status.response = OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT(msg->data.param[LIBOBD2_FRAME_NR_IDX] != request->data.param[LIBOBD2_FRAME_NR_IDX]);

    status = LibOBD2_DecodePid(msg, LIBOBD2_FREEZE_FRAME_IDX, response, responseLen);

exit:
    return status;
}

/******************************************
* title: LibOBD2_DecodeDtcs
* brief: Joins the DTCs of every $03/$07/$0A answer into response as uint16_t,
*        skipping the $0000 fill. Every answer carries exactly 3 DTC slots.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_DecodeDtcs(const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};
    uint16_t dtc = 0;
    size_t outLen = 0;

    OBD2_DEBUG_ASSERT((NULL != buffer) && (NULL != response) && (NULL != responseLen));

    for (size_t msg = 0; msg < bufferLen; msg++)
    {
        // Every answer of the exchange is for the same service
        status.response = OBD_NOT_SUPPORTED;
        OBD2_IF_COND_GOTO_EXIT(buffer[msg].data.sid != buffer[0].data.sid);

        status.response = OBD_ERR_APP_SHORT_MSG;
        OBD2_IF_COND_GOTO_EXIT(LIBOBD2_DTC_MSG_LEN != buffer[msg].dataLen);
        status.response = OBD_STATUS_OK;

        for (size_t slot = 0; slot < LIBOBD2_DTCS_PER_MSG; slot++)
        {
            dtc = LibOBD2_Word(&buffer[msg].data.param[2U * slot]);
            if (0U != dtc)
            {
                outLen += LibOBD2_Put(&response[outLen], &dtc, sizeof(dtc));
            }
        }
    }
    *responseLen = outLen;

exit:
    return status;
}

/******************************************
* title: LibOBD2_DecodeVehicleInfo
* brief: Decodes a $09 (vehicle information) answer into response.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_DecodeVehicleInfo(const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};

    switch (buffer[0].data.param[LIBOBD2_PID_IDX])
    {
        case PID_09_VIN:
        case PID_09_CALID:
        case PID_09_CVN:
        case PID_09_INUSE_PERF_TRACKING:
            *responseLen = LibOBD2_FormulaMultiFrame(buffer, bufferLen, response);
            break;

        default:
            status.response = OBD_NOT_SUPPORTED;
            break;
    }

    return status;
}

/******************************************
* title: LibOBD2_DecodeResponse
* brief: Turns the ECU messages collected for request into the caller's
*        response bytes. An answer for another service or PID reports
*        OBD_NOT_SUPPORTED, so does an unknown service or PID.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_DecodeResponse(const libobd2_data_t *request, const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};
    uint8_t reqSid = 0;

    OBD2_DEBUG_ASSERT((NULL != request) && (NULL != buffer) && (NULL != response) && (NULL != responseLen));

    *responseLen = 0;
    reqSid = request->data.sid;

    // No answer: on ISO 9141 an ECU without DTCs may stay silent, that is 0 DTCs
    status.response = LibOBD2_IsDtcSid(reqSid) ? OBD_STATUS_OK : OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT(0U == bufferLen);

    // The answer must be for this service, and for this PID when the request has one
    status.response = OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT((uint8_t)(reqSid + LIBOBD2_POSITIVE_RESP_OFFSET) != buffer[0].data.sid);
    OBD2_IF_COND_GOTO_EXIT((1U < request->dataLen) &&
                           ((buffer[0].dataLen < 2U) ||
                            (request->data.param[LIBOBD2_PID_IDX] != buffer[0].data.param[LIBOBD2_PID_IDX])));
    status.response = OBD_STATUS_OK;

    switch (reqSid)
    {
        case SID_SHOW_CURRENT_DATA:
            status = LibOBD2_DecodePid(&buffer[0], LIBOBD2_LIVE_DATA_IDX, response, responseLen);
            break;

        case SID_SHOW_FREEZE_FRAME_DATA:
            status = LibOBD2_DecodeFreezeFrame(request, &buffer[0], response, responseLen);
            break;

        case SID_SHOW_STORED_DTCS:
        case SID_SHOW_PENDING_DTCS:
        case SID_PERMANENT_DTCS:
            status = LibOBD2_DecodeDtcs(buffer, bufferLen, response, responseLen);
            break;

        case SID_CLEAR_DTCS:
            // 44: the ECU cleared its emission-related diagnostic information, no data
            break;

        case SID_REQUEST_VEHICLE_INFO:
            status = LibOBD2_DecodeVehicleInfo(buffer, bufferLen, response, responseLen);
            break;

        // Currently not supported
        case SID_TEST_RESULTS_O2_SENSORS:
        case SID_TEST_RESULTS_OTHER:
        case SID_CONTROL_ONBOARD_SYSTEM:
        default:
            status.response = OBD_NOT_SUPPORTED;
            break;
    }

exit:
    return status;
}

/******************************************
* title: LibOBD2_Exchange
* brief: Sends one request and collects its answer (untilSilent: every
*        answer until none starts within P2 max) into answers.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_Exchange(obd_ctx_t *ctx, const obd_data_t *request, size_t requestLen, bool untilSilent, libobd2_data_t *answers, size_t answerCap, size_t *answerCnt)
{
    libobd2_service_t service = { .sidSize = OBD_SHORT_SID, .transferProtocol = TP_READ_ALL };

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != request) && (NULL != answers) && (NULL != answerCnt));

    service.sidSize                    = untilSilent ? OBD_LONG_SID : OBD_SHORT_SID;
    service.serviceRequests[0].data    = *request;
    service.serviceRequests[0].dataLen = requestLen;
    *answerCnt = 0;

    return LibOBD2_DispatchSid(ctx, &service, answers, answerCap, answerCnt);
}

/******************************************
* title: LibOBD2_IsAnswerTo
* brief: True when answer is the positive answer of service sid for TID tid.
******************************************/
OBD2_STATIC bool LibOBD2_IsAnswerTo(const libobd2_data_t *answer, uint8_t sid, uint8_t tid)
{
    return (2U <= answer->dataLen) &&
           ((uint8_t)(sid + LIBOBD2_POSITIVE_RESP_OFFSET) == answer->data.sid) &&
           (tid == answer->data.param[0]);
}

/******************************************
* title: LibOBD2_O2TestScaling
* brief: Scaling per bit and unit of a $05 TID (ISO 15031-5 Annex C).
*        Manufacturer-defined TIDs are left raw.
******************************************/
OBD2_STATIC void LibOBD2_O2TestScaling(uint8_t tid, float *scale, libobd2_unit_t *unit)
{
    *scale = 1.0f;
    *unit  = LIBOBD2_UNIT_NONE;

    if (((0x01U <= tid) && (0x04U >= tid)) || (0x07U == tid) || (0x08U == tid) || ((0x41U <= tid) && (0x4FU >= tid)))
    {
        *scale = 0.005f;
        *unit  = LIBOBD2_UNIT_VOLT;
    }
    else if ((0x05U == tid) || (0x06U == tid) || ((0x21U <= tid) && (0x2FU >= tid)))
    {
        *scale = 0.004f;
        *unit  = LIBOBD2_UNIT_SECOND;
    }
    else if ((0x09U == tid) || (0x0AU == tid) || ((0x30U <= tid) && (0x3FU >= tid)))
    {
        *scale = 0.04f;
        *unit  = LIBOBD2_UNIT_SECOND;
    }
    else if ((0x50U <= tid) && (0x5FU >= tid))
    {
        *scale = 0.05f;
        *unit  = LIBOBD2_UNIT_VOLT;
    }
    else if ((0x61U <= tid) && (0x6FU >= tid))
    {
        *scale = 0.1f;
        *unit  = LIBOBD2_UNIT_HERTZ;
    }
    else if ((0x70U <= tid) && (0x7FU >= tid))
    {
        *unit = LIBOBD2_UNIT_COUNT;
    }
    else
    {
        // $0B-$1F, $81-$FF: reserved or manufacturer specific, left raw
    }
}

/******************************************
* title: LibOBD2_RequestService_Validate
* brief: Checks the arguments of LibOBD2_RequestService. The data link is
*        checked by DL_SendRequest.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_RequestService_Validate(obd_ctx_t *ctx, libobd2_sid_t obdSid, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};

    if ((NULL == ctx) || (NULL == ctx->pDataLink) || (NULL == response) || (NULL == responseLen))
    {
        status.response = OBD_ERR_NULL_PTR;
    }
    else if (obdSid >= OBD2_MAX_NUM_SERVICES)
    {
        status.response = OBD_ERR_INVALID_PARAM;
    }

    return status;
}

/******************************************
* title: LibOBD2_RequestService_Execute
* brief: Runs the service on the data link and decodes the ECU answer into
*        response.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_RequestService_Execute(obd_ctx_t *ctx, libobd2_sid_t obdSid, uint8_t *response, size_t* responseLen)
{
    const libobd2_service_t *service = &libobd2Services[obdSid];
    libobd2_data_t responseBuffer[LIBOBD2_RESP_BUF_MSGS] = {0};
    size_t respBufLen = 0;
    obd_status_t status = {0};

    status = LibOBD2_DispatchSid(ctx, service, responseBuffer, LIBOBD2_RESP_BUF_MSGS, &respBufLen);
    OBD2_ASSERT_OK(status);

    status = LibOBD2_DecodeResponse(LibOBD2_LastRequest(service), responseBuffer, respBufLen, response, responseLen);

exit:
    return status;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: LibOBD2_RequestService
* brief: Requests one OBD service from the ECU and returns its decoded value
*        in response / responseLen.
******************************************/
obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    libobd2_sid_t obdSid,
    uint8_t *response,
    size_t* responseLen)
{
    obd_status_t status = {0};

    status = LibOBD2_RequestService_Validate(ctx, obdSid, response, responseLen);
    OBD2_ASSERT_OK(status);
    status = LibOBD2_RequestService_Execute(ctx, obdSid, response, responseLen);

exit:
    return status;
}

/******************************************
* title: LibOBD2_RequestRaw
* brief: Sends any request and returns every answer as received, until none
*        starts within P2 max.
******************************************/
obd_status_t LibOBD2_RequestRaw(
    obd_ctx_t *ctx,
    const obd_data_t *request,
    size_t requestLen,
    libobd2_data_t *answers,
    size_t answerCap,
    size_t *answerCnt)
{
    obd_status_t status = {0};

    status.response = OBD_ERR_NULL_PTR;
    OBD2_IF_COND_GOTO_EXIT((NULL == ctx) || (NULL == ctx->pDataLink) || (NULL == request) ||
                           (NULL == answers) || (NULL == answerCnt));
    *answerCnt = 0;

    status.response = OBD_ERR_INVALID_PARAM;
    OBD2_IF_COND_GOTO_EXIT((0U == requestLen) || (sizeof(obd_data_t) < requestLen) || (0U == answerCap));
    status.response = OBD_STATUS_OK;

    status = LibOBD2_Exchange(ctx, request, requestLen, true, answers, answerCap, answerCnt);

exit:
    return status;
}

/******************************************
* title: LibOBD2_RequestSupportedTids
* brief: Reads the supported-TIDs bitmask of $05, $06 or $08 for one range.
******************************************/
obd_status_t LibOBD2_RequestSupportedTids(obd_ctx_t *ctx, uint8_t sid, uint8_t range, uint8_t o2Sensor, uint32_t *supported)
{
    obd_data_t request = { .sid = sid, .param = { range } };
    libobd2_data_t answer = {0};
    obd_status_t status = {0};
    size_t requestLen = 0;
    size_t answerCnt = 0;
    size_t maskLen = 0;

    status.response = OBD_ERR_NULL_PTR;
    OBD2_IF_COND_GOTO_EXIT((NULL == ctx) || (NULL == ctx->pDataLink) || (NULL == supported));

    status.response = OBD_ERR_INVALID_PARAM;
    OBD2_IF_COND_GOTO_EXIT(0U != (range % LIBOBD2_TID_RANGE));
    switch (sid)
    {
        case SID_TEST_RESULTS_O2_SENSORS:
            request.param[1] = o2Sensor; /* 05 TID O2S */
            requestLen = 3U;
            break;
        case SID_TEST_RESULTS_OTHER:
            requestLen = 2U;             /* 06 TID */
            break;
        case SID_CONTROL_ONBOARD_SYSTEM:
            requestLen = LIBOBD2_CONTROL_LEN; /* 08 TID 00 00 00 00 00 */
            break;
        default:
            goto exit;
    }
    status.response = OBD_STATUS_OK;

    status = LibOBD2_Exchange(ctx, &request, requestLen, false, &answer, 1U, &answerCnt);
    OBD2_ASSERT_OK(status);

    status.response = OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT(!LibOBD2_IsAnswerTo(&answer, sid, range));
    OBD2_IF_COND_GOTO_EXIT((SID_TEST_RESULTS_O2_SENSORS == sid) && (o2Sensor != answer.data.param[1]));

    status.response = OBD_ERR_APP_SHORT_MSG;
    OBD2_IF_COND_GOTO_EXIT(LIBOBD2_SUPPORTED_TIDS_LEN > answer.dataLen);
    status.response = OBD_STATUS_OK;

    maskLen = LibOBD2_FormulaBitmask(&answer.data.param[LIBOBD2_SUPPORTED_TIDS_IDX], (uint8_t *)supported);
    (void)maskLen;

exit:
    return status;
}

/******************************************
* title: LibOBD2_RequestO2SensorTest
* brief: Reads and scales one $05 test result.
******************************************/
obd_status_t LibOBD2_RequestO2SensorTest(obd_ctx_t *ctx, uint8_t tid, uint8_t o2Sensor, libobd2_o2_test_t *result)
{
    const obd_data_t request = { .sid = SID_TEST_RESULTS_O2_SENSORS, .param = { tid, o2Sensor } };
    libobd2_data_t answer = {0};
    obd_status_t status = {0};
    libobd2_unit_t unit = LIBOBD2_UNIT_NONE;
    size_t answerCnt = 0;
    float scale = 1.0f;

    status.response = OBD_ERR_NULL_PTR;
    OBD2_IF_COND_GOTO_EXIT((NULL == ctx) || (NULL == ctx->pDataLink) || (NULL == result));
    status.response = OBD_STATUS_OK;

    status = LibOBD2_Exchange(ctx, &request, 3U, false, &answer, 1U, &answerCnt);
    OBD2_ASSERT_OK(status);

    status.response = OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT(!LibOBD2_IsAnswerTo(&answer, SID_TEST_RESULTS_O2_SENSORS, tid) || (o2Sensor != answer.data.param[1]));

    status.response = OBD_ERR_APP_SHORT_MSG;
    OBD2_IF_COND_GOTO_EXIT(LIBOBD2_O2_TEST_LEN > answer.dataLen);
    status.response = OBD_STATUS_OK;

    LibOBD2_O2TestScaling(tid, &scale, &unit);
    *result = (libobd2_o2_test_t)
    {
        .tid       = tid,
        .o2Sensor  = o2Sensor,
        .hasLimits = (LIBOBD2_O2_TEST_LIMITS_LEN <= answer.dataLen),
        .rawValue  = answer.data.param[2],
        .unit      = unit,
    };
    if (result->hasLimits)
    {
        result->rawMin = answer.data.param[3];
        result->rawMax = answer.data.param[4];
    }
    result->value  = (float)result->rawValue * scale;
    result->min    = (float)result->rawMin * scale;
    result->max    = (float)result->rawMax * scale;
    result->passed = !result->hasLimits || ((result->rawMin <= result->rawValue) && (result->rawValue <= result->rawMax));

exit:
    return status;
}

/******************************************
* title: LibOBD2_RequestMonitorTests
* brief: Reads every $06 result for one TID, from every ECU and component.
******************************************/
obd_status_t LibOBD2_RequestMonitorTests(obd_ctx_t *ctx, uint8_t tid, libobd2_monitor_test_t *results, size_t resultCap, size_t *resultCnt)
{
    const obd_data_t request = { .sid = SID_TEST_RESULTS_OTHER, .param = { tid } };
    libobd2_data_t answers[LIBOBD2_RESP_BUF_MSGS] = {0};
    obd_status_t status = {0};
    size_t answerCnt = 0;
    uint8_t tltcid = 0;

    status.response = OBD_ERR_NULL_PTR;
    OBD2_IF_COND_GOTO_EXIT((NULL == ctx) || (NULL == ctx->pDataLink) || (NULL == results) || (NULL == resultCnt));
    *resultCnt = 0;
    status.response = OBD_STATUS_OK;

    status = LibOBD2_Exchange(ctx, &request, 2U, true, answers, LIBOBD2_RESP_BUF_MSGS, &answerCnt);
    OBD2_ASSERT_OK(status);

    status.response = OBD_ERR_APP_TOO_MANY_MSGS;
    OBD2_IF_COND_GOTO_EXIT(answerCnt > resultCap);

    for (size_t idx = 0; idx < answerCnt; idx++)
    {
        status.response = OBD_NOT_SUPPORTED;
        OBD2_IF_COND_GOTO_EXIT(!LibOBD2_IsAnswerTo(&answers[idx], SID_TEST_RESULTS_OTHER, tid));
        status.response = OBD_ERR_APP_SHORT_MSG;
        OBD2_IF_COND_GOTO_EXIT(LIBOBD2_MONITOR_TEST_LEN != answers[idx].dataLen);
        status.response = OBD_STATUS_OK;
    }

    for (size_t idx = 0; idx < answerCnt; idx++)
    {
        tltcid = answers[idx].data.param[1];
        results[idx] = (libobd2_monitor_test_t)
        {
            .tid         = tid,
            .componentId = (uint8_t)(tltcid & LIBOBD2_COMPONENT_MASK),
            .limitIsMin  = (0U != (tltcid & LIBOBD2_MIN_LIMIT_MASK)),
            .value       = LibOBD2_Word(&answers[idx].data.param[2]),
            .limit       = LibOBD2_Word(&answers[idx].data.param[4]),
        };
        results[idx].passed = results[idx].limitIsMin ? (results[idx].value >= results[idx].limit)
                                                      : (results[idx].value <= results[idx].limit);
    }
    *resultCnt = answerCnt;

exit:
    return status;
}

/******************************************
* title: LibOBD2_RequestControl
* brief: $08: requests on-board test tid, returns the answer's DATA_A..E.
******************************************/
obd_status_t LibOBD2_RequestControl(obd_ctx_t *ctx, uint8_t tid, const uint8_t *dataIn, uint8_t *dataOut)
{
    obd_data_t request = { .sid = SID_CONTROL_ONBOARD_SYSTEM, .param = { tid } };
    libobd2_data_t answer = {0};
    obd_status_t status = {0};
    size_t answerCnt = 0;

    status.response = OBD_ERR_NULL_PTR;
    OBD2_IF_COND_GOTO_EXIT((NULL == ctx) || (NULL == ctx->pDataLink) || (NULL == dataIn) || (NULL == dataOut));
    status.response = OBD_STATUS_OK;

    memcpy(&request.param[1], dataIn, LIBOBD2_CONTROL_DATA_LEN);
    status = LibOBD2_Exchange(ctx, &request, LIBOBD2_CONTROL_LEN, false, &answer, 1U, &answerCnt);
    OBD2_ASSERT_OK(status);

    status.response = OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT(!LibOBD2_IsAnswerTo(&answer, SID_CONTROL_ONBOARD_SYSTEM, tid));
    status.response = OBD_ERR_APP_SHORT_MSG;
    OBD2_IF_COND_GOTO_EXIT(LIBOBD2_CONTROL_LEN != answer.dataLen);
    status.response = OBD_STATUS_OK;

    memcpy(dataOut, &answer.data.param[1], LIBOBD2_CONTROL_DATA_LEN);

exit:
    return status;
}

