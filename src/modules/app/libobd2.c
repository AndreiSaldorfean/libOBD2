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
/* ============================================ LOCAL VARIABLES ============================================ */
OBD2_STATIC libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES] = {OBD_SUPPORTED_SERVICES};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static obd_status_t dispatchSid(obd_ctx_t* ctx, libobd2_service_t *libobd2Service, libobd2_data_t* response, size_t * respLen)
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
                for (size_t i = 0; i < msg_cnt; i++)
                {
                    status = DL_RecvResponse(pDataLink, &resp, &len);
                    OBD2_ASSERT_OK(status);
                    response[respIdx].data = resp;
                    response[respIdx++].dataLen = len;
                }
            break;
            case TP_READ_UNTIL_TO: // NOTE: Not tested
                while (P2_TIME_MAX != status.timeout)
                {
                    status = DL_RecvResponse(pDataLink, &resp, &len);
                    OBD2_ASSERT_OK(status);
                    response[respIdx].data = resp;
                    response[respIdx++].dataLen = len;
                }
            break;
        }

        tpState = tp[++idx];
    }
    *respLen = respIdx;

exit:
    return status;
}

static void decodeLiveData(libobd2_data_t *buffer, uint8_t *response)
{
    uint8_t pid = buffer[0].data.param[0];

    switch (pid)
    {
        case PID_01_COOLANT_TEMP:
            OP_COOLANT_TEMP(buffer, response);
        break;
    }
}

static void decodeVehicleInfo(libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen)
{
    uint8_t pid = buffer[0].data.param[0];

    switch (pid)
    {
        case PID_09_VIN:
            OP_VIN(buffer, bufferLen, response, responseLen);
        break;
        case PID_09_CALID:
            OP_CALID(buffer, bufferLen, response, responseLen);
        break;
    }
}

static void decodeAndSend(libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen)
{
    uint8_t sid = 0;

    if (bufferLen <= 0)
        return;

    sid = buffer[0].data.sid - 0x40;

    switch (sid)
    {
        case SID_SHOW_CURRENT_DATA:
            decodeLiveData(buffer, response);
            *responseLen = 1;
        break;
        case SID_REQUEST_VEHICLE_INFO:
            decodeVehicleInfo(buffer, bufferLen, response, responseLen);
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
        break;
    }


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
    libobd2_service_t *service = &libobd2Services[obdSid];
    obd_status_t status = {0};
    libobd2_data_t responseBuffer[20] = {0};
    size_t respBufLen = 0;

    (void)service;

    OBD2_CHECK_NULLPTR(response);
    OBD2_CHECK_NULLPTR(responseLen);

    if (obdSid >= OBD2_MAX_NUM_SERVICES)
    {
        status.response = OBD_ERR_INVALID_PARAM;
        goto exit;
    }

    status = dispatchSid(ctx, service, responseBuffer, &respBufLen);
    OBD2_ASSERT_OK(status);

    decodeAndSend(responseBuffer, respBufLen, response, responseLen);

exit:
    return status;
}

