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
    void *pDlHandle = ctx->pDataLinkHandle;
    obd_status_t (*Dl_Connect)(void *handle) = ctx->pDataLink->connect;

    status = Dl_Connect(pDlHandle);
    OBD2_ASSERT_OK(status);

    return status;
}

obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    uint8_t sid,
    uint8_t pid,
    uint32_t *response_buffer,
    size_t *response_len)
{
    obd_response_t response =
    {
        .data = response_buffer,
        .data_len = response_len
    };
    obd_request_t request =
    {
        .sid = sid,
        .param = pid,
        .len = sizeof(sid) + sizeof(pid)
    };
    obd_status_t status = OBD_GENERIC_ERROR;

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->send_request);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->recv_response);
    OBD2_CHECK_NULLPTR(ctx->pDataLinkHandle);
    OBD2_CHECK_NULLPTR(response_buffer);
    OBD2_CHECK_NULLPTR(response_len);

    /* Add aliases to pointers */
    void *pDlHandle = ctx->pDataLinkHandle;
    obd_status_t (*Dl_SendRequest)(void *handle, obd_request_t *req) = ctx->pDataLink->send_request;
    obd_status_t (*Dl_ReceiveResponse)(void *handle, obd_response_t *resp) = ctx->pDataLink->recv_response;

    status = Dl_SendRequest(pDlHandle, &request);
    OBD2_ASSERT_OK(status);

    status = Dl_ReceiveResponse(pDlHandle, &response);

    return status;
}

