/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "datalink.h"
#include "statusRetCodes.h"
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
    obd_status_t status = {0};

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->connect);

    status = DL_Connect(ctx->pDataLink);
exit:
    return status;
}

obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    const obd_request_t* request,
    size_t requestLen,
    obd_response_t* response,
    size_t* responseLen)
{
    obd_status_t status = {0};

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->send_request);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->recv_response);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->pProtocolCtx);
    OBD2_CHECK_NULLPTR(response);
    OBD2_CHECK_NULLPTR(responseLen);

    /* Add aliases to pointers */
    void *self = ctx->pDataLink;

    status = DL_SendRequest(self, request, requestLen);
    OBD2_ASSERT_OK(status);

    status = DL_RecvResponse(self, response, responseLen);

exit:
    return status;
}

