/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "datalink.h"
#include "libobd2_service_requests.h"
#include "libobd2_services.h"
#include "libobd2_transfer_protocol.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <assert.h>
#include <stddef.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
OBD2_STATIC libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES] = {OBD_SUPPORTED_SERVICES};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void dispatchSid(obd_ctx_t* ctx, libobd2_sid_t libobd2Sid, libobd2_data_t* response, size_t * respLen)
{
    libobd2_service_t libobd2Service = libobd2Services[libobd2Sid];
    dataLink_if_t* pDataLink = ctx->pDataLink;
    obd_status_t status;
    obd_data_t resp;
    libobd2_sreq_t *pSreq = &libobd2Service.sreq;

    const uint8_t* tp = libobd2Service.transferProtocol;
    uint8_t idx = 0;
    uint8_t respIdx = 0;
    size_t msg_cnt = 0;
    size_t len = 0;
    uint8_t tpState = tp[idx];

    if (OBD_SMALL_SID == libobd2Service.sidSize)
    {
        status = DL_SendRequest(pDataLink, &pSreq->req[0].data, pSreq->req[0].dataLen);
        OBD2_ASSERT_OK(status);
        status = DL_RecvResponse(ctx->pDataLink, &resp, &len);
        OBD2_ASSERT_OK(status);
    }

    while (TP_END != tpState)
    {
        obd_data_t data = pSreq->req[idx].data;
        size_t dataLen = pSreq->req[idx].dataLen;

        switch (tpState)
        {
            case TP_SEND:
                status = DL_SendRequest(pDataLink, &data, dataLen);
                OBD2_ASSERT_OK(status);
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
                msg_cnt = resp.param[0];
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

        tpState = tp[idx++];
    }
    *respLen = respIdx;

exit:
    return;
}

void listen()
{
    while (1)
    {


    }
}
/* ================================================ MODULE API ============================================= */
obd_status_t ECU_LibOBD2_Init(obd_ctx_t *ctx)
{
    obd_status_t status = {0};

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->connect);

    status = DL_Connect(ctx->pDataLink);

    // listen
exit:
    return status;
}

obd_status_t ECU_LibOBD2_RequestService(
    obd_ctx_t *ctx,
    libobd2_sid_t obdSid,
    uint8_t *response,
    size_t* responseLen)
{
    libobd2_service_t *svc = &libobd2Services[obdSid];
    obd_status_t status = {0};
    libobd2_data_t responseBuffer[255] = {0};
    size_t bufferLen = 0;

    (void)svc;

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->send_request);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->recv_response);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->pProtocolCtx);
    OBD2_CHECK_NULLPTR(response);
    OBD2_CHECK_NULLPTR(responseLen);

    if (obdSid >= OBD2_MAX_NUM_SERVICES)
    {
        status.response = OBD_ERR_INVALID_PARAM;
        goto exit;
    }

    dispatchSid(ctx, obdSid, responseBuffer, &bufferLen);

exit:
    return status;
}

