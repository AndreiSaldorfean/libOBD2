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
/******************************************
* title: L2_ISO9141_ComputeChecksum_Validate
* brief: Checks the inputs of L2_ISO9141_ComputeChecksum.
* input: dataLen - number of data bytes to sum, at most sizeof(obd_data_t)
*        cs      - output for the checksum
* output: obd_status_t - OK, OBD_ERR_NULL_PTR or OBD_ERR_INVALID_PARAM
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
* input: header  - message header
*        data    - sid followed by the params
*        dataLen - number of data bytes to sum
*        cs      - output for the checksum
* output: obd_status_t - always OK
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
* input: cs - computed checksum (unused)
* output: obd_status_t - always OK
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
* input: header  - message header
*        data    - sid followed by the params
*        dataLen - number of data bytes to sum, at most sizeof(obd_data_t)
*        cs      - output for the checksum
* output: obd_status_t - OK, OBD_ERR_NULL_PTR or OBD_ERR_INVALID_PARAM
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
* title: L2_ISO9141_PrepareMessage_Validate
* brief: Checks the inputs of L2_ISO9141_PrepareMessage.
* input: sentMsg  - message to serialize
*        aSentMsg - output buffer
*        dataLen  - sid + params count, 1..ISO9141_MAX_REQ_DATA_LEN
*        len      - output for the number of bytes written
* output: obd_status_t - OK, OBD_ERR_NULL_PTR or OBD_ERR_INVALID_PARAM
******************************************/
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

/******************************************
* title: L2_ISO9141_PrepareMessage_Execute
* brief: Writes header, sid, params and checksum into the wire buffer.
* input: sentMsg  - message to serialize
*        aSentMsg - output buffer, at least dataLen + ISO9141_WIRE_OVERHEAD bytes
*        dataLen  - sid + params count
*        len      - output for the number of bytes written
* output: obd_status_t - always OK
******************************************/
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

/******************************************
* title: L2_ISO9141_PrepareMessage_Verify
* brief: Checks that the frame has exactly header + data + checksum bytes.
* input: dataLen - sid + params count
*        len     - number of bytes written
* output: obd_status_t - OK, or OBD_ERR_COMM_SEND_MSG_FAILED
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_PrepareMessage_Verify(size_t dataLen, size_t *len)
{
    obd_status_t status = {0};

    if (*len != (dataLen + ISO9141_WIRE_OVERHEAD))
    {
        status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    }

    return status;
}

/******************************************
* title: L2_ISO9141_PrepareMessage
* brief: Serializes a message_t into the ISO9141 wire layout
*        (header, sid, params, checksum).
* input: sentMsg  - message to serialize
*        aSentMsg - output buffer, at least dataLen + ISO9141_WIRE_OVERHEAD bytes
*        dataLen  - sid + params count, 1..ISO9141_MAX_REQ_DATA_LEN
*        len      - output for the number of bytes written
* output: obd_status_t - OK, OBD_ERR_NULL_PTR, OBD_ERR_INVALID_PARAM
*         or OBD_ERR_COMM_SEND_MSG_FAILED
******************************************/
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

/******************************************
* title: L2_ISO9141_SendMessage_Validate
* brief: Checks the inputs of L2_ISO9141_SendMessage.
* input: self - data link
*        msg  - bytes to send
*        len  - number of bytes, 1..ISO9141_MAX_WIRE_LEN
* output: obd_status_t - OK, OBD_ERR_NULL_PTR or OBD_ERR_INVALID_PARAM
******************************************/
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

/******************************************
* title: L2_ISO9141_SendMessage_Execute
* brief: Waits P3, sends the bytes with P4 between them, then starts the
*        P2 timeout for the ECU response.
* input: self - data link
*        msg  - bytes to send
*        len  - number of bytes
* output: obd_status_t - always OK
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
* input: self - data link (unused)
* output: obd_status_t - always OK
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
* input: self - data link
*        msg  - bytes to send
*        len  - number of bytes, 1..ISO9141_MAX_WIRE_LEN
* output: obd_status_t - OK, OBD_ERR_NULL_PTR or OBD_ERR_INVALID_PARAM
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
* input: self            - data link, must have a protocol context
*        serviceRequests - sid and params to send
*        dataLen         - sid + params count, 1..ISO9141_MAX_REQ_DATA_LEN
* output: obd_status_t - OK, OBD_ERR_NULL_PTR or OBD_ERR_INVALID_PARAM
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
* input: self            - data link
*        serviceRequests - sid and params to send
*        dataLen         - sid + params count
* output: obd_status_t - OK, or the error of the failed step
******************************************/
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

/******************************************
* title: L2_ISO9141_SendRequest_Verify
* brief: No extra check. L2_ISO9141_SendMessage already verified the send.
* input: self - data link (unused)
* output: obd_status_t - always OK
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
* input: self            - connected data link, must have a protocol context
*        serviceRequests - sid and params to send
*        dataLen         - sid + params count, 1..ISO9141_MAX_REQ_DATA_LEN
* output: obd_status_t - OK, OBD_ERR_NULL_PTR, OBD_ERR_INVALID_PARAM,
*         or the error of the failed send step
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
