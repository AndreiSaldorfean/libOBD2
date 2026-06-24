/* ================================================ INCLUDES =============================================== */
#include "l2_kwp2000.h"
#include "ecu_datalink.h"
#include "statusRetCodes.h"
#include "ecu_l2_iso9141.h"
#include "timing_if.h"
#include "uart_if.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include "l2_kwp2000.h"

/* ================================================= MACROS ================================================ */
#define NUM_SPT_PROTOCOLS (1U)
#define PROTOCOL_NOT_FOUND (NUM_SPT_PROTOCOLS + 1U)
/* ============================================ LOCAL VARIABLES ============================================ */
static dl_connect_t initProtocols[NUM_SPT_PROTOCOLS ] =
{
    [ISO9141] = ecu_l2_iso9141_connect,
};

static dl_send_request_t sendRequests[NUM_SPT_PROTOCOLS] =
{

    [ISO9141] = ecu_l2_iso9141_send_request,
};

static dl_recv_response_t recvResponses[NUM_SPT_PROTOCOLS] =
{

    [ISO9141] = ecu_l2_iso9141_recv_response,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static uint8_t DL_IdentifyProtocol(dataLink_if_t *pDataLink)
{
    obd_status_t status = {0};
    uint8_t protocol = PROTOCOL_NOT_FOUND;

    for (uint8_t protocolIdx = 0; protocolIdx <  NUM_SPT_PROTOCOLS; protocolIdx++)
    {
        status = initProtocols[protocolIdx](pDataLink, &protocol);
        if (status.response == OBD_STATUS_OK)
            return protocol;
    }

    return PROTOCOL_NOT_FOUND;
}

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
obd_status_t ECU_DL_Connect(dataLink_if_t *pDataLink)
{
    obd_status_t status = {0};
    uint8_t protocolIdx = PROTOCOL_NOT_FOUND;
    LIBOBD_TransportInit(pDataLink);
    LIBOBD_TimingInit(pDataLink);

    status.response = OBD_ERR_PROTOCOL_NOT_FOUND;
    protocolIdx = DL_IdentifyProtocol(pDataLink);
    if (protocolIdx == PROTOCOL_NOT_FOUND) return status;

    // Construct datalink interface
    pDataLink->connect       = initProtocols[protocolIdx];
    pDataLink->send_request  = sendRequests[protocolIdx];
    pDataLink->recv_response = recvResponses[protocolIdx];

    status.response = OBD_STATUS_OK;

    return status;
}

obd_status_t ECU_DL_SendRequest(dataLink_if_t *pDataLink, const obd_data_t *serviceRequests, size_t len)
{
    return pDataLink->send_request(pDataLink, serviceRequests, len);
}

obd_status_t ECU_DL_RecvResponse(dataLink_if_t  *pDataLink, obd_data_t *resp, size_t* len)
{
    return pDataLink->recv_response(pDataLink, resp, len);
}
