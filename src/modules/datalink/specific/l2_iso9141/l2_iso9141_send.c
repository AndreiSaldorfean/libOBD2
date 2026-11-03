/* ================================================ INCLUDES =============================================== */
#include "l2_iso9141_priv.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC uint8_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen);
OBD2_STATIC void L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen,size_t *len);
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
OBD2_STATIC uint8_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen)
{
    uint8_t *hdr      = (uint8_t *)&header;
    uint8_t *serviceRequests      = (uint8_t *)&data;
    uint8_t checksum  = 0;
    uint8_t headerLen = 3; /* ISO9141 header is always 3 bytes */

    for (uint8_t idx = 0; idx < headerLen; idx++)
    {
        checksum += hdr[idx];
    }

    for (uint8_t idx = 0; idx < dataLen; idx++)
    {
        checksum += serviceRequests[idx];
    }

    return checksum % 256;
}

OBD2_STATIC void L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len)
{
    size_t idx = 0;

    aSentMsg[idx++] = sentMsg->header.fmt;
    aSentMsg[idx++] = sentMsg->header.trgAddr;
    aSentMsg[idx++] = sentMsg->header.srcAddr;
    aSentMsg[idx++] = sentMsg->data.sid;

    for (uint8_t i = 0; i < dataLen - 1; i++)
    {
        aSentMsg[idx++] = sentMsg->data.param[i];
    }

    aSentMsg[idx++] = sentMsg->cs;

    if (len != NULL)
        *len = idx;
}

OBD2_STATIC obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    obd_status_t status = {0};

    LIBOBD_Delay(self, P3_TIME_MIN);

    for (size_t idx = 0; idx < len; idx++)
    {
        LIBOBD_SendByte(self, msg[idx]);
        LIBOBD_Delay(self, P4_TIME_MIN);
    }

    DL_StartTimeout(self, P2_TIME_MAX);

    // Clear echo
    LIBOBD_FlushRx(self);

    memset(&status, 0, sizeof(obd_status_t));
    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t l2_iso9141_send_request(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    l2_iso9141_ctx_t ctx = *(l2_iso9141_ctx_t *)(self->pProtocolCtx);
    uint8_t aSentMsg[11] = {0};
    obd_status_t status;
    size_t len = 0;
    message_t msg = {0};
    obd_data_t data = *serviceRequests;

    // Construct the message
    msg.header.fmt = ctx.header.fmt;
    msg.header.trgAddr = ctx.header.trgAddr;
    msg.header.srcAddr = ctx.header.srcAddr;

    msg.data = data;
    msg.cs = L2_ISO9141_ComputeChecksum(msg.header, data, dataLen);

    L2_ISO9141_PrepareMessage(&msg, aSentMsg, dataLen, &len);

    status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    status = L2_ISO9141_SendMessage(self, aSentMsg, len);
    OBD2_ASSERT_OK(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}
