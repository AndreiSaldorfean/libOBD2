/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "data_link_if.h"
#include "srv_status.h"
#include "utils.h"
#include <assert.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
obd_status_t LibOBD2_Init(obd_ctx_t *ctx)
{
    obd_status_t status = OBD_GENERIC_ERROR;

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(ctx->pDataLinkHandle);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->connect);

    /* Add aliases to pointers */
    dataLink_if_t *pDlHandle = ctx->pDataLinkHandle;
    obd_status_t (*Dl_Connect)(dataLink_if_t *handle) = ctx->pDataLink->connect;

    status = Dl_Connect(pDlHandle);
    OBD2_ASSERT_OK(status);

    return status;
}

obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    const obd_request_t* request,
    size_t requestLen,
    obd_response_t* response,
    size_t* responseLen)
{
    obd_status_t status = OBD_GENERIC_ERROR;

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->send_request);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->recv_response);
    OBD2_CHECK_NULLPTR(ctx->pDataLinkHandle);
    OBD2_CHECK_NULLPTR(response);
    OBD2_CHECK_NULLPTR(responseLen);

    /* Add aliases to pointers */
    void *pDlHandle = ctx->pDataLinkHandle;
    obd_status_t (*Dl_SendRequest)(dataLink_if_t *handle, const obd_request_t *req, size_t len) = ctx->pDataLink->send_request;
    obd_status_t (*Dl_ReceiveResponse)(dataLink_if_t  *handle, obd_response_t *resp, size_t* len) = ctx->pDataLink->recv_response;

    status = Dl_SendRequest(pDlHandle, request, requestLen);
    OBD2_ASSERT_OK(status);

    status = Dl_ReceiveResponse(pDlHandle, response, responseLen);

    return status;
}

