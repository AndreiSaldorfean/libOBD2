/* ================================================ INCLUDES =============================================== */
#include "l2_kwp2000.h"
#include "datalink.h"
#include "statusRetCodes.h"
#include "l2_iso9141.h"
#include "timing_if.h"
#include "uart_if.h"
#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "l2_kwp2000.h"

/* ================================================= MACROS ================================================ */
#define NUM_SPT_PROTOCOLS (2U)
#define PROTOCOL_NOT_FOUND (NUM_SPT_PROTOCOLS + 1U)
/* ============================================ LOCAL VARIABLES ============================================ */
static dl_connect_t initProtocols[NUM_SPT_PROTOCOLS ] =
{
    [ISO9141] = l2_iso9141_connect,
    [KWP2000] = l2_kwp_connect,
};

static dl_send_request_t sendRequests[NUM_SPT_PROTOCOLS] =
{

    [ISO9141] = l2_iso9141_send_request,
    [KWP2000] = l2_kwp_send_request,
};

static dl_recv_response_t recvResponses[NUM_SPT_PROTOCOLS] =
{

    [ISO9141] = l2_iso9141_recv_response,
    [KWP2000] = l2_kwp_recv_response,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static obd_status_t DL_IdentifyProtocol(dataLink_if_t *pDataLink, uint8_t* protocol)
{
    obd_status_t status = {0};

    for (uint8_t idx = 0; idx <  NUM_SPT_PROTOCOLS; idx++)
    {
        status = initProtocols[idx](pDataLink, protocol);
        if (status.response == OBD_STATUS_OK)
            return status;
    }

    return status;
}

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
obd_status_t DL_Connect(dataLink_if_t *pDataLink)
{
    obd_status_t status = {0};
    uint8_t protocolIdx = PROTOCOL_NOT_FOUND;
    LIBOBD_TransportInit(pDataLink);
    LIBOBD_TimingInit(pDataLink);

    status = DL_IdentifyProtocol(pDataLink, &protocolIdx);
    if (status.response != OBD_STATUS_OK) return status;

    // Construct datalink interface
    pDataLink->connect       = initProtocols[protocolIdx];
    pDataLink->send_request  = sendRequests[protocolIdx];
    pDataLink->recv_response = recvResponses[protocolIdx];

    status.response = OBD_STATUS_OK;

    return status;
}

obd_status_t DL_SendRequest(dataLink_if_t *pDataLink, const obd_data_t *serviceRequests, size_t dataLen)
{
    return pDataLink->send_request(pDataLink, serviceRequests, dataLen);
}

obd_status_t DL_RecvResponse(dataLink_if_t  *pDataLink, obd_data_t *resp, size_t* dataLen)
{
    return pDataLink->recv_response(pDataLink, resp, dataLen);
}
