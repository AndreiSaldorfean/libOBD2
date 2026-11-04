/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "l2_iso9141_priv.h"
#include "statusRetCodes.h"
#include "utils.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC obd_status_t L2_ISO9141_WaitForResponseStart(dataLink_if_t *self, uint8_t *respByte);

OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Validate(dataLink_if_t *self, message_t *recvdMsg, size_t* len);
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Execute(dataLink_if_t *self, message_t *recvdMsg, size_t* len);
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Verify(size_t* len);
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len);

OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Validate(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen);
OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Execute(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen);
OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Verify(obd_data_t *resp);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: L2_ISO9141_WaitForResponseStart
* brief: Waits for the first byte (format byte) of an ECU response inside
*        the P2 window, then stops the P2 timeout.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_WaitForResponseStart(dataLink_if_t *self, uint8_t *respByte)
{
    obd_status_t status = {0};

    // Format byte
    while (!LIBOBD_ReceiveByte(self, respByte))
    {
        // Check for P2 Timeout from Tester to ECU
        status.timeout = OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU;
        OBD2_ASSERT_EQUAL_OR_EXIT(false, LIBOBD_IsTimeoutExpired(self));
    }

    status.timeout = OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU;
    OBD2_ASSERT_EQUAL_OR_EXIT(true, DL_IsTimeoutMinDone(self, P2_TIME_MIN));
    LIBOBD_StopTimeout(self);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

/******************************************
* title: L2_ISO9141_RecvMessage_Validate
* brief: Checks the inputs of L2_ISO9141_RecvMessage.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Validate(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
{
    obd_status_t status = L2_ISO9141_ValidateLinkWithCtx(self);
    OBD2_ASSERT_OK(status);

    if ((NULL == recvdMsg) || (NULL == len))
    {
        status.response = OBD_ERR_NULL_PTR;
    }

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_RecvMessage_Execute
* brief: Reads the frame byte by byte until one equals the running checksum,
*        skipping header.len if absent. Restarts the P2 timeout at the end.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Execute(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
{
    l2_iso9141_ctx_t ctx = *(l2_iso9141_ctx_t *)self->pProtocolCtx;
    uint8_t *pMsg       = (uint8_t *)recvdMsg;
    obd_status_t status = {0};
    uint8_t byteRead    = 0;
    uint8_t byteReadIdx = 0;
    uint8_t runningCheckSum = 0;
    uint32_t elapsedTime = 0;
    bool hasLenByte      = false;

    status = L2_ISO9141_WaitForResponseStart(self, &byteRead);
    status.response = OBD_ERR_COMM_FMT_BYTE_NOT_RECVD;
    OBD2_ASSERT_NO_TIMEOUT(status);

    runningCheckSum = byteRead;
    pMsg[byteReadIdx++] = byteRead;

    // Only KWP2000 frames with no length in the format byte carry a length byte
    hasLenByte = (KWP2000 == ctx.protocol) && (0U == (recvdMsg->header.fmt & 0x3FU));

    // Bound by the message_t layout (header slot incl. len + data), not the wire length,
    // since byteReadIdx skips header.len when the frame has no length byte
    while ((sizeof(header_t) + ISO9141_MAX_DATA_FIELD_LEN) > byteReadIdx)
    {
        // Receive byte
        status = DL_ReadByteInTimeframe(self, &elapsedTime , &byteRead, P1_TIME_MIN, P1_TIME_MAX);
        status.response = OBD_ERR_COMM_DATA_BYTE_NOT_RECVD;
        OBD2_ASSERT_NO_TIMEOUT(status);

        // Once the header is complete, a byte equal to the running checksum ends the frame
        if ((sizeof(header_t) <= byteReadIdx) && (byteRead == runningCheckSum))
        {
            /* This byte is the checksum — frame complete */
            recvdMsg->cs = byteRead;
            *len = byteReadIdx - sizeof(header_t);
            memset(&status, 0, sizeof(obd_status_t));
            goto exit;
        }

        // Construct frame (message_t)
        pMsg[byteReadIdx++] = byteRead;
        runningCheckSum += byteRead;

        // Frames without a length byte leave the header.len slot empty
        if ((3U == byteReadIdx) && !hasLenByte)
        {
            byteReadIdx++;
        }
    }

    status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
exit:
    DL_StartTimeout(self, P2_TIME_MAX);
    return status;
}

/******************************************
* title: L2_ISO9141_RecvMessage_Verify
* brief: Checks that the frame carried at least a sid and fits the data field.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Verify(size_t* len)
{
    obd_status_t status = {0};

    if ((0U == *len) || (*len > ISO9141_MAX_DATA_FIELD_LEN))
    {
        status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
    }

    return status;
}

/******************************************
* title: L2_ISO9141_RecvMessage
* brief: Receives one ISO9141 frame from the ECU.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
{
    obd_status_t status = {0};

    status = L2_ISO9141_RecvMessage_Validate(self, recvdMsg, len);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_RecvMessage_Execute(self, recvdMsg, len);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_RecvMessage_Verify(len);

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_RecvResponse_Validate
* brief: Checks the inputs of l2_iso9141_recv_response.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Validate(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen)
{
    obd_status_t status = L2_ISO9141_ValidateLinkWithCtx(self);
    OBD2_ASSERT_OK(status);

    if ((NULL == resp) || (NULL == dataLen))
    {
        status.response = OBD_ERR_NULL_PTR;
    }

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_RecvResponse_Execute
* brief: Receives one frame and copies its data part to resp.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Execute(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    status = L2_ISO9141_RecvMessage(self, &recvdMsg, dataLen);
    OBD2_ASSERT_OK(status);

    *resp = recvdMsg.data;

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

/******************************************
* title: L2_ISO9141_RecvResponse_Verify
* brief: Checks that the ECU did not answer with a negative response (0x7F).
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Verify(obd_data_t *resp)
{
    obd_status_t status = {0};

    if (resp->sid == 0x7F)
    {
        status.response = OBD_ERR_COMM_ECU_RESPONSE_7F;
    }

    return status;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: l2_iso9141_recv_response
* brief: Receives the ECU response to the last request over ISO9141-2.
******************************************/
obd_status_t l2_iso9141_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen)
{
    obd_status_t status = {0};

    status = L2_ISO9141_RecvResponse_Validate(self, resp, dataLen);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_RecvResponse_Execute(self, resp, dataLen);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_RecvResponse_Verify(resp);

exit:
    return status;
}
