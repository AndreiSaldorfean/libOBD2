/* ================================================ INCLUDES =============================================== */
#include "l2_iso9141_priv.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC obd_status_t L2_ISO9141_ReadHeader_Validate(dataLink_if_t *self, header_t *header);
OBD2_STATIC obd_status_t L2_ISO9141_ReadHeader_Execute(dataLink_if_t *self, header_t *header);
OBD2_STATIC obd_status_t L2_ISO9141_ReadHeader_Verify(header_t *header);
OBD2_STATIC OBD2_INLINE obd_status_t L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header);

OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Validate(dataLink_if_t *self, message_t *recvdMsg, size_t* len);
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Execute(dataLink_if_t *self, message_t *recvdMsg, size_t* len);
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Verify(size_t* len);
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len);

OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Validate(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen);
OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Execute(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen);
OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Verify(obd_data_t *resp);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: L2_ISO9141_ReadHeader_Validate
* brief: Checks the inputs of L2_ISO9141_ReadHeader.
* input: self   - data link
*        header - output for the received header
* output: obd_status_t - OK, or OBD_ERR_NULL_PTR
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_ReadHeader_Validate(dataLink_if_t *self, header_t *header)
{
    obd_status_t status = L2_ISO9141_ValidateLink(self);
    OBD2_ASSERT_OK(status);

    if (NULL == header)
    {
        status.response = OBD_ERR_NULL_PTR;
    }

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_ReadHeader_Execute
* brief: Waits for the format byte inside the P2 window, then reads the
*        target and source bytes with P1 timing.
* input: self   - data link, with the P2 timeout already running
*        header - output for the received header
* output: obd_status_t - OK, or the OBD_ERR_COMM_* code of the missing byte
*         together with its timeout
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_ReadHeader_Execute(dataLink_if_t *self, header_t *header)
{
    uint8_t *buffer     = (uint8_t *)header;
    obd_status_t status = {0};

    // Format byte
    while (!LIBOBD_ReceiveByte(self, buffer))
    {
        // Check for P2 Timeout from Tester to ECU
        status.response = OBD_ERR_COMM_FMT_BYTE_NOT_RECVD;
        status.timeout = OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU;
        OBD2_ASSERT_EQUAL_OR_EXIT(false, LIBOBD_IsTimeoutExpired(self));
    }

    status.timeout = OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU;
    OBD2_ASSERT_EQUAL_OR_EXIT(true, DL_IsTimeoutMinDone(self, P2_TIME_MIN));
    LIBOBD_StopTimeout(self);

    // Target byte
    status = DL_ReadByteInTimeframe(self, NULL, buffer + 1, P1_TIME_MIN, P1_TIME_MAX);
    status.response = OBD_ERR_COMM_TRGT_BYTE_NOT_RECVD;
    OBD2_ASSERT_NO_TIMEOUT(status);

    // Source byte
    status = DL_ReadByteInTimeframe(self, NULL, buffer + 2, P1_TIME_MIN, P1_TIME_MAX);
    status.response = OBD_ERR_COMM_SRC_BYTE_NOT_RECVD;
    OBD2_ASSERT_NO_TIMEOUT(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

/******************************************
* title: L2_ISO9141_ReadHeader_Verify
* brief: No extra check. The header bytes are not fixed for a response, so
*        the checksum in L2_ISO9141_RecvMessage is what catches bad bytes.
* input: header - received header (unused)
* output: obd_status_t - always OK
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_ReadHeader_Verify(header_t *header)
{
    obd_status_t status = {0};
    (void)header;
    return status;
}

/******************************************
* title: L2_ISO9141_ReadHeader
* brief: Reads the 3-byte ISO9141 header of an ECU response.
* input: self   - data link, with the P2 timeout already running
*        header - output for the received header
* output: obd_status_t - OK, OBD_ERR_NULL_PTR, or the OBD_ERR_COMM_* code of
*         the missing byte together with its timeout
******************************************/
OBD2_STATIC OBD2_INLINE obd_status_t L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header)
{
    obd_status_t status = {0};

    status = L2_ISO9141_ReadHeader_Validate(self, header);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_ReadHeader_Execute(self, header);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_ReadHeader_Verify(header);

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_RecvMessage_Validate
* brief: Checks the inputs of L2_ISO9141_RecvMessage.
* input: self     - data link
*        recvdMsg - output for the received message
*        len      - output for the number of data bytes
* output: obd_status_t - OK, or OBD_ERR_NULL_PTR
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Validate(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
{
    obd_status_t status = L2_ISO9141_ValidateLink(self);
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
* brief: Reads the header, then data bytes until one equals the running
*        checksum, which ends the frame. Restarts the P2 timeout at the end.
* input: self     - data link, with the P2 timeout already running
*        recvdMsg - output for the received message
*        len      - output for the number of data bytes (sid included)
* output: obd_status_t - OK, the OBD_ERR_COMM_* code of the missing byte,
*         or OBD_ERR_COMM_RECV_MSG_FAILED when no checksum is found
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Execute(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
{
    uint8_t *pMsg       = (uint8_t *)recvdMsg;
    obd_status_t status = {0};
    uint8_t idx         = 0;
    uint8_t byte        = 0;
    uint8_t runningCheckSum = 0;
    uint32_t elapsedTime = 0;

    status = L2_ISO9141_ReadHeader(self, &recvdMsg->header);
    OBD2_ASSERT_OK(status);

    runningCheckSum = recvdMsg->header.fmt
                    + recvdMsg->header.trgAddr
                    + recvdMsg->header.srcAddr;

    while (ISO9141_MAX_DATA_FIELD > idx)
    {
        status = DL_ReadByteInTimeframe(self, &elapsedTime , &byte, P1_TIME_MIN, P1_TIME_MAX);
        status.response = OBD_ERR_COMM_DATA_BYTE_NOT_RECVD;
        OBD2_ASSERT_NO_TIMEOUT(status);

        if (byte == runningCheckSum)
        {
            /* This byte is the checksum — frame complete */
            recvdMsg->cs = byte;
            *len = idx;
            memset(&status, 0, sizeof(obd_status_t));
            goto exit;
        }

        pMsg[sizeof(header_t) + idx] = byte;
        runningCheckSum += byte;
        idx++;
    }

    status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
exit:
    DL_StartTimeout(self, P2_TIME_MAX);
    return status;
}

/******************************************
* title: L2_ISO9141_RecvMessage_Verify
* brief: Checks that the frame carried at least a sid and fits the data field.
* input: len - number of data bytes received
* output: obd_status_t - OK, or OBD_ERR_COMM_RECV_MSG_FAILED
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage_Verify(size_t* len)
{
    obd_status_t status = {0};

    if ((0U == *len) || (*len > ISO9141_MAX_DATA_FIELD))
    {
        status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
    }

    return status;
}

/******************************************
* title: L2_ISO9141_RecvMessage
* brief: Receives one ISO9141 frame from the ECU.
* input: self     - data link, with the P2 timeout already running
*        recvdMsg - output for the received message
*        len      - output for the number of data bytes (sid included)
* output: obd_status_t - OK, OBD_ERR_NULL_PTR, the OBD_ERR_COMM_* code of the
*         missing byte, or OBD_ERR_COMM_RECV_MSG_FAILED
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
* input: self    - data link
*        resp    - output for the response data
*        dataLen - output for the number of data bytes
* output: obd_status_t - OK, or OBD_ERR_NULL_PTR
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_RecvResponse_Validate(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen)
{
    obd_status_t status = L2_ISO9141_ValidateLink(self);
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
* input: self    - data link
*        resp    - output for the response data
*        dataLen - output for the number of data bytes
* output: obd_status_t - OK, or the error of L2_ISO9141_RecvMessage
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
* input: resp - received response data
* output: obd_status_t - OK, or OBD_ERR_COMM_ECU_RESPONSE_7F
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
* input: self    - data link, after l2_iso9141_send_request
*        resp    - output for the response data (sid + params)
*        dataLen - output for the number of data bytes
* output: obd_status_t - OK, OBD_ERR_NULL_PTR, OBD_ERR_COMM_ECU_RESPONSE_7F,
*         or the error of the failed receive step
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
