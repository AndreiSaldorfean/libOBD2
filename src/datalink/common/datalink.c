/* ================================================ INCLUDES =============================================== */
#include "kwp2000.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "statusRetCodes.h"
#include "iso9141.h"
#include "timing_if.h"
#include "uart_if.h"
#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "kwp2000.h"

/* ================================================= MACROS ================================================ */
#define NUM_SPT_PROTOCOLS (2U)
#define PROTOCOL_NOT_FOUND (NUM_SPT_PROTOCOLS + 1U)
/* ============================================ LOCAL VARIABLES ============================================ */
static dl_connect_t initProtocols[NUM_SPT_PROTOCOLS ] =
{
    [ISO9141] = ISO9141_Connect,
    [KWP2000] = KWP_Connect,
};

static dl_send_request_t sendRequests[NUM_SPT_PROTOCOLS] =
{

    [ISO9141] = ISO9141_SendRequest,
    [KWP2000] = KWP_SendRequest,
};

static dl_recv_response_t recvResponses[NUM_SPT_PROTOCOLS] =
{

    [ISO9141] = ISO9141_RecvResponse,
    [KWP2000] = KWP_RecvResponse,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static obd_status_t DL_ValidateLink(const dataLink_if_t *pDataLink);
static obd_status_t DL_IdentifyProtocol(dataLink_if_t *pDataLink, uint8_t* protocol);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: DL_ValidateLink
* brief: Checks the UART and timer ops, their handles and the protocol context
*        that every protocol needs. Protocol layers rely on this check.
******************************************/
static obd_status_t DL_ValidateLink(const dataLink_if_t *pDataLink)
{
    obd_status_t status = {0};

    if ((NULL == pDataLink)
     || (NULL == pDataLink->pUartOps)
     || (NULL == pDataLink->pTransportHandle)
     || (NULL == pDataLink->pTimingOps)
     || (NULL == pDataLink->pTimingHandle)
     || (NULL == pDataLink->pProtocolCtx))
    {
        status.response = OBD_ERR_NULL_PTR;
    }

    return status;
}

static obd_status_t DL_IdentifyProtocol(dataLink_if_t *pDataLink, uint8_t* protocol)
{
    obd_status_t status = {0};

    for (uint8_t idx = 0; idx <  NUM_SPT_PROTOCOLS; idx++)
    {
        status = initProtocols[idx](pDataLink, protocol);
        if (status.response == OBD_STATUS_OK)
            goto exit;
    }

exit:
    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t DL_Connect(dataLink_if_t *pDataLink)
{
    obd_status_t status = {0};
    uint8_t protocolIdx = PROTOCOL_NOT_FOUND;

    status = DL_ValidateLink(pDataLink);
    OBD2_ASSERT_OK(status);

    status.response = OBD_ERR_CONN_TRANS_INIT_FAILED;
    OBD2_IF_COND_GOTO_EXIT(!LIBOBD2_UART_Init(pDataLink));
    status.response = OBD_ERR_CONN_TIMER_INIT_FAILED;
    OBD2_IF_COND_GOTO_EXIT(OBD_TMR_OK != LIBOBD2_TMR_Init(pDataLink));

    status = DL_IdentifyProtocol(pDataLink, &protocolIdx);
    OBD2_ASSERT_OK(status);

    // Construct datalink interface
    pDataLink->connect       = initProtocols[protocolIdx];
    pDataLink->send_request  = sendRequests[protocolIdx];
    pDataLink->recv_response = recvResponses[protocolIdx];

    status.response = OBD_STATUS_OK;
exit:
    return status;
}

obd_status_t DL_SendRequest(dataLink_if_t *pDataLink, const obd_data_t *serviceRequests, size_t dataLen)
{
    obd_status_t status = DL_ValidateLink(pDataLink);
    OBD2_ASSERT_OK(status);

    // Not connected yet
    OBD2_CHECK_NULLPTR(pDataLink->send_request);

    status = pDataLink->send_request(pDataLink, serviceRequests, dataLen);
exit:
    return status;
}

obd_status_t DL_RecvResponse(dataLink_if_t  *pDataLink, obd_data_t *resp, size_t* dataLen)
{
    obd_status_t status = DL_ValidateLink(pDataLink);
    OBD2_ASSERT_OK(status);

    // Not connected yet
    OBD2_CHECK_NULLPTR(pDataLink->recv_response);

    status = pDataLink->recv_response(pDataLink, resp, dataLen);
exit:
    return status;
}
