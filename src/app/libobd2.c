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

/* ============================================ LOCAL VARIABLES ============================================ */
OBD2_STATIC const libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES] = {OBD_SUPPORTED_SERVICES};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC obd_status_t LibOBD2_DispatchSid(obd_ctx_t* ctx, const libobd2_service_t *libobd2Service, libobd2_data_t* response, size_t respCap, size_t * respLen);
OBD2_STATIC obd_status_t LibOBD2_DecodeLiveData(const libobd2_data_t *buffer, uint8_t *response, size_t* responseLen);
OBD2_STATIC obd_status_t LibOBD2_DecodeVehicleInfo(const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen);
OBD2_STATIC obd_status_t LibOBD2_DecodeResponse(const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen);

OBD2_STATIC obd_status_t LibOBD2_Init_Validate(obd_ctx_t *ctx);
OBD2_STATIC obd_status_t LibOBD2_Init_Execute(obd_ctx_t *ctx);

OBD2_STATIC obd_status_t LibOBD2_RequestService_Validate(obd_ctx_t *ctx, libobd2_sid_t obdSid, uint8_t *response, size_t* responseLen);
OBD2_STATIC obd_status_t LibOBD2_RequestService_Execute(obd_ctx_t *ctx, libobd2_sid_t obdSid, uint8_t *response, size_t* responseLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
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

            case TP_READ_UNTIL_TO: // NOTE: Not tested
                #if 0 /* TODO: fix  the implementation here*/
                while (P2_TIME_MAX != status.timeout)
                {
                    status = DL_RecvResponse(pDataLink, &resp, &len);
                    OBD2_ASSERT_OK(status);
                    response[respIdx].data = resp;
                    response[respIdx++].dataLen = len;
                }
                #endif /* 0 */
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
* title: LibOBD2_DecodeLiveData
* brief: Decodes a $01 (current data) response into response.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_DecodeLiveData(const libobd2_data_t *buffer, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};

    switch (buffer[0].data.param[LIBOBD2_PID_IDX])
    {
        case PID_01_COOLANT_TEMP:
            response[0]  = LibOBD2_FormulaCoolantTemp(&buffer[0]);
            *responseLen = 1;
            break;

        default:
            status.response = OBD_NOT_SUPPORTED;
            break;
    }

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
* brief: Turns the collected ECU messages into the caller's response bytes.
*        Unknown services/PIDs report OBD_NOT_SUPPORTED.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_DecodeResponse(const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};
    uint8_t sid = 0;

    OBD2_DEBUG_ASSERT((NULL != buffer) && (NULL != response) && (NULL != responseLen));

    *responseLen = 0;

    status.response = OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT(0U == bufferLen);

    sid = buffer[0].data.sid - LIBOBD2_POSITIVE_RESP_OFFSET;

    switch (sid)
    {
        case SID_SHOW_CURRENT_DATA:
            status = LibOBD2_DecodeLiveData(buffer, response, responseLen);
            break;

        case SID_REQUEST_VEHICLE_INFO:
            status = LibOBD2_DecodeVehicleInfo(buffer, bufferLen, response, responseLen);
            break;

        // Currently not supported
        case SID_SHOW_FREEZE_FRAME_DATA:
        case SID_SHOW_STORED_DTCS:
        case SID_CLEAR_DTCS:
        case SID_TEST_RESULTS_O2_SENSORS:
        case SID_TEST_RESULTS_OTHER:
        case SID_SHOW_PENDING_DTCS:
        case SID_CONTROL_ONBOARD_SYSTEM:
        case SID_PERMANENT_DTCS:
        default:
            status.response = OBD_NOT_SUPPORTED;
            break;
    }

exit:
    return status;
}

/******************************************
* title: LibOBD2_Init_Validate
* brief: Checks the arguments of LibOBD2_Init.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_Init_Validate(obd_ctx_t *ctx)
{
    obd_status_t status = {0};

    if ((NULL == ctx) || (NULL == ctx->pDataLink))
    {
        status.response = OBD_ERR_NULL_PTR;
    }

    return status;
}

/******************************************
* title: LibOBD2_Init_Execute
* brief: Connects the data link (protocol detection and init).
******************************************/
OBD2_STATIC obd_status_t LibOBD2_Init_Execute(obd_ctx_t *ctx)
{
    return DL_Connect(ctx->pDataLink);
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

    status = LibOBD2_DecodeResponse(responseBuffer, respBufLen, response, responseLen);

exit:
    return status;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: LibOBD2_Init
* brief: Connects to the ECU over the context's data link.
******************************************/
obd_status_t LibOBD2_Init(obd_ctx_t *ctx)
{
    obd_status_t status = {0};

    status = LibOBD2_Init_Validate(ctx);
    OBD2_ASSERT_OK(status);
    status = LibOBD2_Init_Execute(ctx);

exit:
    return status;
}

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
