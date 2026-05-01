/* ================================================ INCLUDES =============================================== */
#include "ecu_uart.h"
#include "l2_iso9141.h"
#include "datalink.h"
#include "libobd2.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC uint8_t ECU_L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t len);
OBD2_STATIC obd_status_t ECU_L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t ECU_L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len);
OBD2_STATIC OBD2_INLINE obd_status_t ECU_L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header);
OBD2_STATIC void ECU_L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len, size_t dataLen);

static inline void startTimeout(dataLink_if_t *self, uint32_t timeout);
static inline bool isTimeoutMinDone(dataLink_if_t *self, uint32_t minTimeout);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static inline bool isTimeoutMinDone(dataLink_if_t *self, uint32_t minTimeout)
{
    uint32_t p2TimeElapsed = 0;

    p2TimeElapsed = LIBOBD_GetTimeMs(self);
    p2TimeElapsed -= LIBOBD_GetTimeSample(self);

    return p2TimeElapsed > minTimeout;
}

static inline void startTimeout(dataLink_if_t *self, uint32_t timeout)
{
    LIBOBD_SetTimeSample(self, LIBOBD_GetTimeMs(self));
    LIBOBD_StartTimeout(self, timeout);
}

OBD2_STATIC uint8_t ECU_L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t len)
{
    uint8_t *hdr      = (uint8_t *)&header;
    uint8_t *req      = (uint8_t *)&data;
    uint8_t checksum  = 0;
    uint8_t headerLen = 3; /* ISO9141 header is always 3 bytes */

    for (uint8_t idx = 0; idx < headerLen; idx++)
    {
        checksum += hdr[idx];
    }

    for (uint8_t idx = 0; idx < len + 1; idx++)
    {
        checksum += req[idx];
    }

    return checksum % 256;
}

OBD2_STATIC OBD2_INLINE obd_status_t ECU_L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header)
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
    OBD2_ASSERT_EQUAL_OR_EXIT(true, isTimeoutMinDone(self, P2_TIME_MIN));

    // Target byte
    status.response = OBD_ERR_COMM_TRGT_BYTE_NOT_RECVD;
    status = ReadByteInTimeframe(self, buffer + 1, P4_TIME_MIN, P4_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // Source byte
    status.response = OBD_ERR_COMM_SRC_BYTE_NOT_RECVD;
    status = ReadByteInTimeframe(self, buffer + 2, P4_TIME_MIN, P4_TIME_MAX);
    OBD2_ASSERT_OK(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC obd_status_t ECU_L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    obd_status_t status = {0};

    LIBOBD_Delay(self, P2_TIME_MIN);

    for (size_t idx = 0; idx < len; idx++)
    {
        LIBOBD_SendByte(self, msg[idx]);
        LIBOBD_Delay(self, P1_TIME_MIN);
    }

    startTimeout(self, P1_TIME_MAX);

    // Clear echo
    LIBOBD_FlushRx(self);

    memset(&status, 0, sizeof(obd_status_t));
    return status;
}

OBD2_STATIC obd_status_t ECU_L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
{
    uint8_t *pMsg       = (uint8_t *)recvdMsg;
    obd_status_t status = {0};
    uint8_t idx         = 0;

    status = ECU_L2_ISO9141_ReadHeader(self, &recvdMsg->header);
    OBD2_ASSERT_OK(status);

    // Read data
    while(OBD_ERR_TIMEOUT_MAX != status.timeout)
    {
        status = ReadByteInTimeframe(self, pMsg + idx + 4, P4_TIME_MIN, P4_TIME_MAX);
        idx++;
    }
    *len = idx;

    // Remove cs from data field
    recvdMsg->cs = pMsg[idx+2];
    pMsg[idx+2] = 0x0;

    startTimeout(self, P2_TIME_MAX);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC void ECU_L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len, size_t dataLen)
{
    size_t idx = 0;

    aSentMsg[idx++] = sentMsg->header.fmt;
    aSentMsg[idx++] = sentMsg->header.trgAddr;
    aSentMsg[idx++] = sentMsg->header.srcAddr;
    aSentMsg[idx++] = sentMsg->data.sid;

    for (size_t i = 0; i < dataLen; i++)
    {
        aSentMsg[idx++] = sentMsg->data.param[i];
    }

    aSentMsg[idx++] = sentMsg->cs;

    if (len != NULL)
        *len = idx;
}

OBD2_STATIC obd_status_t ECU_L2_ISO9141_5BaudInit(dataLink_if_t *self)
{
    l2_iso9141_ctx_t ctx = *(l2_iso9141_ctx_t *)(self->pProtocolCtx);
    obd_status_t status = {0};
    (void)status;
    (void)ctx;
    uint8_t syncByte = 0;
    uint8_t kb2Inverted = 0;

    status.response = 0x255;
    printf("[ECU_5B] begin\n");
    // Read wake-up byte at 5 baudRate
    if(!ECUSIM_ReadByteBitBanged(self, &syncByte, 5))
    {
        printf("[ECU_5B] fail: bit-banged read returned false\n");
        goto exit;
    }

    printf("[ECU_5B] syncByte=0x%02X\n", syncByte);

    OBD2_ASSERT_EQUAL_OR_EXIT(0x33, syncByte);
    LIBOBD_Delay(self, ISO9141_W1_TIME_MIN);
    printf("[ECU_5B] send 0x55\n");

    // Send sync byte
    LIBOBD_SendByte(self, 0x55);

    // Send KB1
    LIBOBD_Delay(self, ISO9141_W2_TIME_MIN);
    printf("[ECU_5B] send KB1=0x08\n");
    LIBOBD_SendByte(self, 0x08);

    // Send KB2
    printf("[ECU_5B] send KB2=0x08\n");
    LIBOBD_SendByte(self, 0x08);

    // Receive kb2 inverted
    status = ReadByteInTimeframe(self, &kb2Inverted, 0, ISO9141_W4_TIME_MAX);
    printf("[ECU_5B] kb2Inverted=0x%02X timeout=0x%04X resp=0x%04X\n", kb2Inverted, status.timeout, status.response);
    OBD2_ASSERT_EQUAL_OR_EXIT((uint8_t)~0x08, kb2Inverted);

    // Send inverted address
    LIBOBD_Delay(self, ISO9141_W4_TIME_MIN);
    printf("[ECU_5B] send invAddr=0x%02X\n", (uint8_t)~0x33);
    LIBOBD_SendByte(self, ~0x33);

    ctx.header.fmt = 0x42;
    ctx.header.trgAddr = 0x6B;
    ctx.header.srcAddr = 0x12;

    memset(&status, 0, sizeof(obd_status_t));
    printf("[ECU_5B] success\n");
exit:
    if ((status.timeout != 0U) || (status.response != 0U))
    {
        printf("[ECU_5B] exit fail timeout=0x%04X resp=0x%04X\n", status.timeout, status.response);
    }
    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t ecu_l2_iso9141_connect(dataLink_if_t *self, uint8_t* protocol)
{
    l2_iso9141_ctx_t  *ctx = (l2_iso9141_ctx_t *)(self->pProtocolCtx);
    memset(ctx, 0, sizeof(l2_iso9141_ctx_t));
    *protocol = ISO9141;

    printf("here3\n");
    return ECU_L2_ISO9141_5BaudInit(self);
}

obd_status_t ecu_l2_iso9141_send_request(dataLink_if_t *self, const obd_data_t *req, size_t len)
{
    l2_iso9141_ctx_t ctx = *(l2_iso9141_ctx_t *)(self->pProtocolCtx);
    uint8_t aMessage[11] = {0};
    obd_status_t status;
    size_t msgLen = 0;
    message_t message = {0};

    obd_data_t data = *req;

    // Construct the message
    message.header.fmt = ctx.header.fmt;
    message.header.trgAddr = ctx.header.trgAddr;
    message.header.srcAddr = ctx.header.srcAddr;

    message.data = data;
    message.cs = ECU_L2_ISO9141_ComputeChecksum(message.header, data, len);

    ECU_L2_ISO9141_PrepareMessage(&message, aMessage, &msgLen, len);

    status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    status = ECU_L2_ISO9141_SendMessage(self, aMessage, msgLen);
    OBD2_ASSERT_OK(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

obd_status_t ecu_l2_iso9141_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t *len)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
    status = ECU_L2_ISO9141_RecvMessage(self, &recvdMsg, len);
    OBD2_ASSERT_OK(status);

    *resp = recvdMsg.data;

    if (resp->sid != 0x7F)
    {
        status.response = OBD_ERR_COMM_ECU_RESPONSE_7F;
        goto exit;
    }

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}
