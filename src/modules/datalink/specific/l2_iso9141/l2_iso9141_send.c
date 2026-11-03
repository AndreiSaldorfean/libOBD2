/* ================================================ INCLUDES =============================================== */
#include "l2_iso9141_priv.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum_Validate(size_t dataLen, uint8_t *cs);
OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum_Execute(header_t header, obd_data_t data, size_t dataLen, uint8_t *cs);
OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum_Verify(uint8_t *cs);
OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen, uint8_t *cs);

OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage_Validate(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);
OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage_Execute(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);
OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage_Verify(size_t dataLen, size_t *len);
OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);

OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Validate(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Execute(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Verify(dataLink_if_t *self);
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);

OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Validate(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);
OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Execute(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);
OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Verify(dataLink_if_t *self);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum_Validate(size_t dataLen, uint8_t *cs)
{
    obd_status_t status = {0};

    if (NULL == cs)
    {
        status.response = OBD_ERR_NULL_PTR;
    }
    else if (dataLen > sizeof(obd_data_t))
    {
        status.response = OBD_ERR_INVALID_PARAM;
    }

    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum_Execute(header_t header, obd_data_t data, size_t dataLen, uint8_t *cs)
{
    obd_status_t status = {0};
    uint8_t *hdr      = (uint8_t *)&header;
    uint8_t *serviceRequests      = (uint8_t *)&data;
    uint8_t checksum  = 0;
    uint8_t headerLen = ISO9141_HEADER_LEN; /* ISO9141 header is always 3 bytes */

    for (uint8_t idx = 0; idx < headerLen; idx++)
    {
        checksum += hdr[idx];
    }

    for (uint8_t idx = 0; idx < dataLen; idx++)
    {
        checksum += serviceRequests[idx];
    }

    *cs = checksum % 256;
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum_Verify(uint8_t *cs)
{
    obd_status_t status = {0};
    (void)cs;
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen, uint8_t *cs)
{
    obd_status_t status = {0};

    status = L2_ISO9141_ComputeChecksum_Validate(dataLen, cs);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_ComputeChecksum_Execute(header, data, dataLen, cs);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_ComputeChecksum_Verify(cs);

exit:
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage_Validate(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len)
{
    obd_status_t status = {0};

    if ((NULL == sentMsg) || (NULL == aSentMsg) || (NULL == len))
    {
        status.response = OBD_ERR_NULL_PTR;
    }
    else if ((0U == dataLen) || (dataLen > ISO9141_MAX_REQ_DATA_LEN))
    {
        status.response = OBD_ERR_INVALID_PARAM;
    }

    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage_Execute(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len)
{
    obd_status_t status = {0};
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

    *len = idx;
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage_Verify(size_t dataLen, size_t *len)
{
    obd_status_t status = {0};

    if (*len != (dataLen + ISO9141_WIRE_OVERHEAD))
    {
        status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    }

    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len)
{
    obd_status_t status = {0};

    status = L2_ISO9141_PrepareMessage_Validate(sentMsg, aSentMsg, dataLen, len);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_PrepareMessage_Execute(sentMsg, aSentMsg, dataLen, len);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_PrepareMessage_Verify(dataLen, len);

exit:
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Validate(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    obd_status_t status = L2_ISO9141_ValidateLink(self);
    OBD2_ASSERT_OK(status);

    if (NULL == msg)
    {
        status.response = OBD_ERR_NULL_PTR;
    }
    else if ((0U == len) || (len > ISO9141_MAX_WIRE_LEN))
    {
        status.response = OBD_ERR_INVALID_PARAM;
    }

exit:
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Execute(dataLink_if_t *self, uint8_t *msg, size_t len)
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

OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Verify(dataLink_if_t *self)
{
    obd_status_t status = {0};
    (void)self;
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    obd_status_t status = {0};

    status = L2_ISO9141_SendMessage_Validate(self, msg, len);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_SendMessage_Execute(self, msg, len);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_SendMessage_Verify(self);

exit:
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Validate(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    obd_status_t status = L2_ISO9141_ValidateLinkWithCtx(self);
    OBD2_ASSERT_OK(status);

    if (NULL == serviceRequests)
    {
        status.response = OBD_ERR_NULL_PTR;
    }
    else if ((0U == dataLen) || (dataLen > ISO9141_MAX_REQ_DATA_LEN))
    {
        status.response = OBD_ERR_INVALID_PARAM;
    }

exit:
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Execute(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    l2_iso9141_ctx_t ctx = *(l2_iso9141_ctx_t *)(self->pProtocolCtx);
    uint8_t aSentMsg[ISO9141_MAX_WIRE_LEN] = {0};
    obd_status_t status;
    size_t len = 0;
    message_t msg = {0};
    obd_data_t data = *serviceRequests;

    // Construct the message
    msg.header.fmt = ctx.header.fmt;
    msg.header.trgAddr = ctx.header.trgAddr;
    msg.header.srcAddr = ctx.header.srcAddr;

    msg.data = data;
    status = L2_ISO9141_ComputeChecksum(msg.header, data, dataLen, &msg.cs);
    OBD2_ASSERT_OK(status);

    status = L2_ISO9141_PrepareMessage(&msg, aSentMsg, dataLen, &len);
    OBD2_ASSERT_OK(status);

    status = L2_ISO9141_SendMessage(self, aSentMsg, len);
    OBD2_ASSERT_OK(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Verify(dataLink_if_t *self)
{
    obd_status_t status = {0};
    (void)self;
    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t l2_iso9141_send_request(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    obd_status_t status = {0};

    status = L2_ISO9141_SendRequest_Validate(self, serviceRequests, dataLen);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_SendRequest_Execute(self, serviceRequests, dataLen);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_SendRequest_Verify(self);

exit:
    return status;
}
