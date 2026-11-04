/* ================================================ INCLUDES =============================================== */
#include "iso9141_priv.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC uint8_t ISO9141_ComputeChecksum(const header_t *header, const obd_data_t *data, size_t dataLen);
OBD2_STATIC size_t ISO9141_SerializeMessage(const message_t *msg, uint8_t *frame, size_t dataLen);
OBD2_STATIC obd_status_t ISO9141_SendMessage(dataLink_if_t *self, const uint8_t *frame, size_t frameLen);

OBD2_STATIC obd_status_t ISO9141_SendRequest_Validate(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);
OBD2_STATIC obd_status_t ISO9141_SendRequest_Execute(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: ISO9141_ComputeChecksum
* brief: Returns the ISO9141 checksum: the sum of the 3 header bytes and
*        dataLen data bytes, modulo 256.
******************************************/
OBD2_STATIC uint8_t ISO9141_ComputeChecksum(const header_t *header, const obd_data_t *data, size_t dataLen)
{
    const uint8_t *dataBytes = (const uint8_t *)data;
    uint8_t checksum = 0;

    OBD2_DEBUG_ASSERT((NULL != header) && (NULL != data) && (dataLen <= sizeof(obd_data_t)));

    checksum = header->fmt + header->trgAddr + header->srcAddr;

    for (size_t idx = 0; idx < dataLen; idx++)
    {
        checksum += dataBytes[idx];
    }

    return checksum;
}

/******************************************
* title: ISO9141_SerializeMessage
* brief: Writes header, sid, params and checksum into frame and returns the
*        number of bytes written (dataLen + ISO9141_WIRE_OVERHEAD).
******************************************/
OBD2_STATIC size_t ISO9141_SerializeMessage(const message_t *msg, uint8_t *frame, size_t dataLen)
{
    size_t idx = 0;

    OBD2_DEBUG_ASSERT((NULL != msg) && (NULL != frame));
    OBD2_DEBUG_ASSERT((0U < dataLen) && (dataLen <= ISO9141_MAX_REQ_DATA_LEN));

    // fill the header
    frame[idx++] = msg->header.fmt;
    frame[idx++] = msg->header.trgAddr;
    frame[idx++] = msg->header.srcAddr;
    frame[idx++] = msg->data.sid;

    // fill the data
    for (size_t i = 0; i < dataLen - 1U; i++)
    {
        frame[idx++] = msg->data.param[i];
    }

    // fill the checksum
    frame[idx++] = msg->cs;

    return idx;
}

/******************************************
* title: ISO9141_SendMessage
* brief: Waits P3, sends the frame with P4 between bytes, then starts the
*        P2 timeout for the ECU response.
******************************************/
OBD2_STATIC obd_status_t ISO9141_SendMessage(dataLink_if_t *self, const uint8_t *frame, size_t frameLen)
{
    obd_status_t status = {0};

    OBD2_DEBUG_ASSERT((NULL != self) && (NULL != frame));
    OBD2_DEBUG_ASSERT((0U < frameLen) && (frameLen <= ISO9141_MAX_FRAME_LEN));

    LIBOBD2_TMR_Delay(self, P3_TIME_MIN);

    for (size_t idx = 0; idx < frameLen; idx++)
    {
        LIBOBD2_UART_SendByte(self, frame[idx]);
        LIBOBD2_TMR_Delay(self, P4_TIME_MIN);
    }

    status = DL_StartTimeout(self, P2_TIME_MAX);

    // Clear echo
    // TODO: compare the echo with the frame instead of dropping it
    LIBOBD2_UART_FlushRx(self);

    return status;
}

/******************************************
* title: ISO9141_SendRequest_Validate
* brief: Checks the arguments of ISO9141_SendRequest. The link itself is
*        checked by DL_SendRequest.
******************************************/
OBD2_STATIC obd_status_t ISO9141_SendRequest_Validate(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    obd_status_t status = {0};

    OBD2_DEBUG_ASSERT((NULL != self) && (NULL != self->pProtocolCtx));

    if (NULL == serviceRequests)
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
* title: ISO9141_SendRequest_Execute
* brief: Builds the request with the header from the context, adds the
*        checksum, serializes it and sends it.
******************************************/
OBD2_STATIC obd_status_t ISO9141_SendRequest_Execute(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    const iso9141_ctx_t *ctx = (const iso9141_ctx_t *)(self->pProtocolCtx);
    message_t msg = { .header = ctx->header, .data = *serviceRequests };
    uint8_t frame[ISO9141_MAX_FRAME_LEN] = {0};
    size_t frameLen = 0;

    msg.cs   = ISO9141_ComputeChecksum(&msg.header, &msg.data, dataLen);
    frameLen = ISO9141_SerializeMessage(&msg, frame, dataLen);

    return ISO9141_SendMessage(self, frame, frameLen);
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: ISO9141_SendRequest
* brief: Sends an OBD service request to the ECU over ISO9141-2.
******************************************/
obd_status_t ISO9141_SendRequest(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    obd_status_t status = {0};

    status = ISO9141_SendRequest_Validate(self, serviceRequests, dataLen);
    OBD2_ASSERT_OK(status);
    status = ISO9141_SendRequest_Execute(self, serviceRequests, dataLen);

exit:
    return status;
}
