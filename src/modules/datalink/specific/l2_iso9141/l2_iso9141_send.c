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

OBD2_STATIC obd_status_t L2_ISO9141_SerializeMessage_Validate(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);
OBD2_STATIC obd_status_t L2_ISO9141_SerializeMessage_Execute(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);
OBD2_STATIC obd_status_t L2_ISO9141_SerializeMessage_Verify(size_t dataLen, size_t *len);
OBD2_STATIC obd_status_t L2_ISO9141_SerializeMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);

OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Validate(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Execute(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Verify(dataLink_if_t *self);
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);

OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Validate(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);
OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Execute(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);
OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Verify(dataLink_if_t *self);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: L2_ISO9141_ComputeChecksum_Validate
* brief: Checks the inputs of L2_ISO9141_ComputeChecksum.
******************************************/
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

/******************************************
* title: L2_ISO9141_ComputeChecksum_Execute
* brief: Sums the 3 header bytes and dataLen data bytes, modulo 256.
******************************************/
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

/******************************************
* title: L2_ISO9141_ComputeChecksum_Verify
* brief: No extra check. Any 8-bit value is a valid checksum.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_ComputeChecksum_Verify(uint8_t *cs)
{
    obd_status_t status = {0};
    (void)cs;
    return status;
}

/******************************************
* title: L2_ISO9141_ComputeChecksum
* brief: Computes the ISO9141 checksum of a message: the sum of the header
*        and data bytes, modulo 256.
******************************************/
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

/******************************************
* title: L2_ISO9141_SerializeMessage_Validate
* brief: Checks the inputs of L2_ISO9141_SerializeMessage.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_SerializeMessage_Validate(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len)
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

/******************************************
* title: L2_ISO9141_SerializeMessage_Execute
* brief: Writes header, sid, params and checksum into the wire buffer.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_SerializeMessage_Execute(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len)
{
    obd_status_t status = {0};
    size_t idx = 0;

    // fill the header
    aSentMsg[idx++] = sentMsg->header.fmt;
    aSentMsg[idx++] = sentMsg->header.trgAddr;
    aSentMsg[idx++] = sentMsg->header.srcAddr;
    aSentMsg[idx++] = sentMsg->data.sid;

    // fill the data
    for (uint8_t i = 0; i < dataLen - 1; i++)
    {
        aSentMsg[idx++] = sentMsg->data.param[i];
    }

    // fill the checksum
    aSentMsg[idx++] = sentMsg->cs;

    *len = idx;
    return status;
}

/******************************************
* title: L2_ISO9141_SerializeMessage_Verify
* brief: Checks that the frame has exactly header + data + checksum bytes.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_SerializeMessage_Verify(size_t dataLen, size_t *len)
{
    obd_status_t status = {0};

    if (*len != (dataLen + ISO9141_WIRE_OVERHEAD))
    {
        status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    }

    return status;
}

/******************************************
* title: L2_ISO9141_SerializeMessage
* brief: Serializes a message_t into the ISO9141 wire layout
*        (header, sid, params, checksum).
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_SerializeMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len)
{
    obd_status_t status = {0};

    status = L2_ISO9141_SerializeMessage_Validate(sentMsg, aSentMsg, dataLen, len);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_SerializeMessage_Execute(sentMsg, aSentMsg, dataLen, len);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_SerializeMessage_Verify(dataLen, len);

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_SendMessage_Validate
* brief: Checks the inputs of L2_ISO9141_SendMessage.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Validate(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    obd_status_t status = L2_ISO9141_ValidateLink(self);
    OBD2_ASSERT_OK(status);

    if (NULL == msg)
    {
        status.response = OBD_ERR_NULL_PTR;
    }
    else if ((0U == len) || (len > ISO9141_MAX_FRAME_LEN))
    {
        status.response = OBD_ERR_INVALID_PARAM;
    }

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_SendMessage_Execute
* brief: Waits P3, sends the bytes with P4 between them, then starts the
*        P2 timeout for the ECU response.
******************************************/
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

/******************************************
* title: L2_ISO9141_SendMessage_Verify
* brief: No extra check yet. Checking the echo needs LIBOBD_FlushRx, which
*        is still a stub.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage_Verify(dataLink_if_t *self)
{
    obd_status_t status = {0};
    (void)self;
    return status;
}

/******************************************
* title: L2_ISO9141_SendMessage
* brief: Sends a serialized frame on the K-line with ISO9141 timing.
******************************************/
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

/******************************************
* title: L2_ISO9141_SendRequest_Validate
* brief: Checks the inputs of l2_iso9141_send_request.
******************************************/
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

/******************************************
* title: L2_ISO9141_SendRequest_Execute
* brief: Builds the request with the header from the context, adds the
*        checksum, serializes it and sends it.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Execute(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    const l2_iso9141_ctx_t ctx = *(l2_iso9141_ctx_t *)(self->pProtocolCtx);
    message_t msg = { .header = ctx.header, .data = *serviceRequests };
    uint8_t serializedMsg[ISO9141_MAX_FRAME_LEN] = {0};
    size_t serializedMsgLen = 0;
    obd_status_t status;

    status = L2_ISO9141_ComputeChecksum(msg.header, *serviceRequests, dataLen, &msg.cs);
    OBD2_ASSERT_OK(status);

    status = L2_ISO9141_SerializeMessage(&msg, serializedMsg, dataLen, &serializedMsgLen);
    OBD2_ASSERT_OK(status);

    status = L2_ISO9141_SendMessage(self, serializedMsg, serializedMsgLen);
    OBD2_ASSERT_OK(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

/******************************************
* title: L2_ISO9141_SendRequest_Verify
* brief: No extra check. L2_ISO9141_SendMessage already verified the send.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_SendRequest_Verify(dataLink_if_t *self)
{
    obd_status_t status = {0};
    (void)self;
    return status;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: l2_iso9141_send_request
* brief: Sends an OBD service request to the ECU over ISO9141-2.
******************************************/
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
