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
/* ============================================ LOCAL VARIABLES ============================================ */
OBD2_STATIC const ecu_libobd2_service_t ecuResponses[ECU_MAX_SID][ECU_MAX_PID] = {ECU_SRV_RESP};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
/******************************************
* title: ECU_LibOBD2_GetAnswer
* brief: The answer of the simulated ECU to request sid / pid (pid 0 for the
*        services without one), NULL when it does not answer.
******************************************/
const ecu_libobd2_service_t *ECU_LibOBD2_GetAnswer(uint8_t sid, uint8_t pid)
{
    const ecu_libobd2_service_t *answer = NULL;

    if ((ECU_MAX_SID > sid) && (ECU_MAX_PID > pid) && (0U < ecuResponses[sid][pid].sreqLen))
    {
        answer = &ecuResponses[sid][pid];
    }

    return answer;
}

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
    const ecu_libobd2_service_t *answer = NULL;
    obd_data_t resp = {0};
    obd_status_t status;
    size_t dataLen = 0;

    status = ECU_DL_RecvResponse(ctx->pDataLink, &resp, &dataLen);
    OBD2_ASSERT_OK(status);

    // Unsupported requests get no answer (ISO 15031-5 5.2.4.1)
    answer = ECU_LibOBD2_GetAnswer(resp.sid, resp.param[0]);
    OBD2_IF_COND_GOTO_EXIT(NULL == answer);

    for (size_t idx = 0; idx < answer->sreqLen; idx++)
    {
        const libobd2_data_t *data = &answer->serviceRequests.serviceRequests[idx];

        status = ECU_DL_SendRequest(ctx->pDataLink, &data->data, data->dataLen);
        OBD2_ASSERT_OK(status);
    }

exit:
    return status;
}
