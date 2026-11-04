/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "datalink.h"
#include "iso15031_5.h"
#include "libobd2_formulas.h"
#include "libobd2_service_requests.h"
#include "libobd2_services.h"
#include "libobd2_transfer_protocol.h"
#include "libobd2_formulas.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

/* ================================================= MACROS ================================================ */
#define LIBOBD2_RESP_BUF_MSGS  (20U) /* ECU messages one request can collect */

/* ============================================ LOCAL VARIABLES ============================================ */
OBD2_STATIC libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES] = {OBD_SUPPORTED_SERVICES};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static obd_status_t LibOBD2_DispatchSid(obd_ctx_t* ctx, libobd2_service_t *libobd2Service, libobd2_data_t* response, size_t * respLen)
{
    dataLink_if_t* pDataLink = ctx->pDataLink;
    obd_status_t status = {0};
    obd_data_t resp;
    libobd2_data_t *pServiceRequests = libobd2Service->serviceRequests;

    const uint8_t* tp = libobd2Service->transferProtocol;
    uint8_t idx = 0;
    uint8_t respIdx = 0;
    size_t msg_cnt = 0;
    size_t len = 0;
    uint8_t tpState = tp[idx];
    size_t msgIdx = 0;

    if (OBD_SHORT_SID == libobd2Service->sidSize)
    {
        status = DL_SendRequest(pDataLink, &pServiceRequests[0].data, pServiceRequests[0].dataLen);
        OBD2_ASSERT_OK(status);
        status = DL_RecvResponse(ctx->pDataLink, &resp, &len);
        OBD2_ASSERT_OK(status);

        response->data = resp;
        response->dataLen = len;
        *respLen = 1;
        goto exit;
    }

    while (TP_END != tpState)
    {
        obd_data_t data = pServiceRequests[msgIdx].data;
        size_t dataLen = pServiceRequests[msgIdx].dataLen;

        switch (tpState)
        {
            case TP_SEND:
                status = DL_SendRequest(pDataLink, &data, dataLen);
                OBD2_ASSERT_OK(status);
                msgIdx++;
                break;

            case TP_RECV:
                status = DL_RecvResponse(pDataLink, &resp, &len);
                OBD2_ASSERT_OK(status);
                response[respIdx].data = resp;
                response[respIdx++].dataLen = len;
            break;
            case TP_MSG_CNT:
                status = DL_RecvResponse(pDataLink, &resp, &len);
                OBD2_ASSERT_OK(status);
                msg_cnt = resp.param[1];
            break;
            case TP_MSGS:
                // The ECU announced the count; refuse more messages than the buffer holds
                status.response = OBD_ERR_APP_TOO_MANY_MSGS;
                OBD2_IF_COND_GOTO_EXIT(msg_cnt > (LIBOBD2_RESP_BUF_MSGS - respIdx));
                status.response = OBD_STATUS_OK;

                for (size_t i = 0; i < msg_cnt; i++)
                {
                    status = DL_RecvResponse(pDataLink, &resp, &len);
                    OBD2_ASSERT_OK(status);
                    response[respIdx].data = resp;
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
        }

        tpState = tp[++idx];
    }
    *respLen = respIdx;

exit:
    return status;
}

static obd_status_t LibOBD2_DecodeLiveData(libobd2_data_t *buffer, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};
    uint8_t pid = buffer[0].data.param[0];

    switch (pid)
    {
        case PID_01_COOLANT_TEMP:
            OP_COOLANT_TEMP(buffer, response);
            *responseLen = 1;
        break;
        default:
            status.response = OBD_NOT_SUPPORTED;
        break;
    }

    return status;
}

static obd_status_t LibOBD2_DecodeVehicleInfo(libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};
    uint8_t pid = buffer[0].data.param[0];

    switch (pid)
    {
        case PID_09_VIN:
            OP_VIN(buffer, bufferLen, response, responseLen);
        break;
        case PID_09_CALID:
            OP_CALID(buffer, bufferLen, response, responseLen);
        break;
        default:
            status.response = OBD_NOT_SUPPORTED;
        break;
    }

    return status;
}

static obd_status_t LibOBD2_DecodeAndSend(libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen)
{
    obd_status_t status = {0};
    uint8_t sid = 0;

    *responseLen = 0;

    if (0U == bufferLen)
    {
        status.response = OBD_NOT_SUPPORTED;
        return status;
    }

    sid = buffer[0].data.sid - 0x40;

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

    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t LibOBD2_Init(obd_ctx_t *ctx)
{
    obd_status_t status = {0};

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);

    status = DL_Connect(ctx->pDataLink);
exit:
    return status;
}

obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    libobd2_sid_t obdSid,
    uint8_t *response,
    size_t* responseLen)
{
    libobd2_service_t *service = NULL;
    obd_status_t status = {0};
    libobd2_data_t responseBuffer[LIBOBD2_RESP_BUF_MSGS] = {0};
    size_t respBufLen = 0;

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(response);
    OBD2_CHECK_NULLPTR(responseLen);

    if (obdSid >= OBD2_MAX_NUM_SERVICES)
    {
        status.response = OBD_ERR_INVALID_PARAM;
        goto exit;
    }
    service = &libobd2Services[obdSid];

    status = LibOBD2_DispatchSid(ctx, service, responseBuffer, &respBufLen);
    OBD2_ASSERT_OK(status);

    status = LibOBD2_DecodeAndSend(responseBuffer, respBufLen, response, responseLen);

exit:
    return status;
}

