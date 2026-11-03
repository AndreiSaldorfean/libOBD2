/* ================================================ INCLUDES =============================================== */
#include "l2_iso9141_priv.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC OBD2_INLINE obd_status_t L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header);
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
OBD2_STATIC OBD2_INLINE obd_status_t L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header)
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

OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
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

/* ================================================ MODULE API ============================================= */
obd_status_t l2_iso9141_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
    status = L2_ISO9141_RecvMessage(self, &recvdMsg, dataLen);
    OBD2_ASSERT_OK(status);

    *resp = recvdMsg.data;

    if (resp->sid == 0x7F)
    {
        status.response = OBD_ERR_COMM_ECU_RESPONSE_7F;
        goto exit;
    }

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}
