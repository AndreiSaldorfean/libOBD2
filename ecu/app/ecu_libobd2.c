/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "ecu_datalink.h"
#include "libobd2_service_requests.h"
#include "libobd2_services.h"
#include "libobd2_transfer_protocol.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "ecu_libobd2_services.h"

/* ================================================= MACROS ================================================ */
#define ECU_MAX_SIDS (10U) /* rows of ecuResponses, indexed by sid */
#define ECU_MAX_PIDS (10U) /* columns of ecuResponses, indexed by pid */

/* ============================================ LOCAL VARIABLES ============================================ */
ecu_libobd2_service_t ecuResponses[ECU_MAX_SIDS][ECU_MAX_PIDS] = {ECU_SRV_RESP};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
obd_status_t ECU_LibOBD2_Init(obd_ctx_t *ctx)
{
    obd_status_t status = {0};

    OBD2_CHECK_NULLPTR(ctx);
    OBD2_CHECK_NULLPTR(ctx->pDataLink);
    OBD2_CHECK_NULLPTR(ctx->pDataLink->connect);

    status = ECU_DL_Connect(ctx->pDataLink);
    OBD2_ASSERT_OK(status);


exit:
    return status;
}

obd_status_t ECU_Listen(obd_ctx_t* ctx)
{
    obd_data_t resp = {0};
    obd_status_t status;
    size_t dataLen = 0;

    status = ECU_DL_RecvResponse(ctx->pDataLink, &resp, &dataLen);
    OBD2_ASSERT_OK(status);

    // Requests outside the response table get no answer (a real ECU stays silent too)
    status.response = OBD_NOT_SUPPORTED;
    OBD2_IF_COND_GOTO_EXIT((resp.sid >= ECU_MAX_SIDS) || (resp.param[0] >= ECU_MAX_PIDS));
    status.response = OBD_STATUS_OK;

    for (size_t idx = 0; idx < ecuResponses[resp.sid][resp.param[0]].sreqLen; idx++)
    {
        const libobd2_data_t *data = &ecuResponses[resp.sid][resp.param[0]].serviceRequests.serviceRequests[idx];

        status = ECU_DL_SendRequest(ctx->pDataLink, &data->data, data->dataLen);
        OBD2_ASSERT_OK(status);
    }

exit:
    return status;
}
