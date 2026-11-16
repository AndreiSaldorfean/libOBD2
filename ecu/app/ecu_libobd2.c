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
#include "ecu_libobd2.h"

/* ================================================= MACROS ================================================ */
#define ECU_MAX_SIDS (10U) /* rows of ecuResponses, indexed by sid */
#define ECU_MAX_PIDS (10U) /* columns of ecuResponses, indexed by pid */

/* ============================================ LOCAL VARIABLES ============================================ */
ecu_libobd2_service_t ecuResponses[ECU_MAX_SIDS][ECU_MAX_PIDS] = {ECU_SRV_RESP};
static ecu_request_hook_t requestHook = NULL;
static void *requestHookUserData = NULL;

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

    // Let the simulation update its values before they are sent
    if (NULL != requestHook)
    {
        requestHook(requestHookUserData, &resp);
    }

    for (size_t idx = 0; idx < ecuResponses[resp.sid][resp.param[0]].sreqLen; idx++)
    {
        const libobd2_data_t *data = &ecuResponses[resp.sid][resp.param[0]].serviceRequests.serviceRequests[idx];

        status = ECU_DL_SendRequest(ctx->pDataLink, &data->data, data->dataLen);
        OBD2_ASSERT_OK(status);
    }

exit:
    return status;
}

/******************************************
* title: ECU_SetResponseParam
* brief: Changes one parameter byte of a simulated response, e.g. the A byte
*        of $01 $05 (coolant temperature). Call it from the thread that runs
*        ECU_Listen, between requests.
******************************************/
obd_status_t ECU_SetResponseParam(uint8_t sid, uint8_t pid, size_t msgIdx, size_t paramIdx, uint8_t value)
{
    obd_status_t status = {0};

    if ((sid >= ECU_MAX_SIDS) || (pid >= ECU_MAX_PIDS) || (msgIdx >= LIBOBD2_MAX_SREQ)
     || (paramIdx >= sizeof(((obd_data_t *)0)->param)))
    {
        status.response = OBD_ERR_INVALID_PARAM;
    }
    else
    {
        ecuResponses[sid][pid].serviceRequests.serviceRequests[msgIdx].data.param[paramIdx] = value;
    }

    return status;
}

/******************************************
* title: ECU_SetRequestHook
* brief: Registers a function called for every request the ECU answers,
*        after it is received and before the response is sent. NULL removes it.
******************************************/
void ECU_SetRequestHook(ecu_request_hook_t hook, void *pUserData)
{
    requestHook         = hook;
    requestHookUserData = pUserData;
}
