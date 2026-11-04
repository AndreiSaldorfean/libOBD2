/* ================================================ INCLUDES =============================================== */
#include "iso9141.h"
#include "datalink.h"
#include "datalink_utils.h"
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
OBD2_STATIC uint8_t ECU_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t len);
OBD2_STATIC obd_status_t ECU_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t ECU_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len);
OBD2_STATIC OBD2_INLINE obd_status_t ECU_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header);
OBD2_STATIC void ECU_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len, size_t dataLen);

static inline void startTimeout(dataLink_if_t *self, uint32_t timeout);
static inline bool isTimeoutMinDone(dataLink_if_t *self, uint32_t minTimeout);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static inline bool isTimeoutMinDone(dataLink_if_t *self, uint32_t minTimeout)
{
    uint32_t p2TimeElapsed = 0;

    p2TimeElapsed = LIBOBD2_TMR_GetTimeMs(self);
    p2TimeElapsed -= LIBOBD2_TMR_GetTimeSample(self);

    return p2TimeElapsed >= minTimeout;
}

static inline void startTimeout(dataLink_if_t *self, uint32_t timeout)
{
    LIBOBD2_TMR_SetTimeSample(self, LIBOBD2_TMR_GetTimeMs(self));
    LIBOBD2_TMR_StartTimeout(self, timeout);
}

OBD2_STATIC uint8_t ECU_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t len)
{
    uint8_t *hdr      = (uint8_t *)&header;
    uint8_t *serviceRequests      = (uint8_t *)&data;
    uint8_t checksum  = 0;
    uint8_t headerLen = 3; /* ISO9141 header is always 3 bytes */

    for (uint8_t idx = 0; idx < headerLen; idx++)
    {
        checksum += hdr[idx];
    }

    for (uint8_t idx = 0; idx < len; idx++)
    {
        checksum += serviceRequests[idx];
    }

    return checksum % 256;
}

OBD2_STATIC OBD2_INLINE obd_status_t ECU_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header)
{
    uint8_t *buffer     = (uint8_t *)header;
    obd_status_t status = {0};

    // Format byte
    if (!DL_RecvByteBlocking(self, buffer))
    {
        status.response = OBD_ERR_COMM_ECU_CONNECTION_CLSD;
        goto exit;
    }

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

OBD2_STATIC obd_status_t ECU_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    obd_status_t status = {0};

    LIBOBD2_TMR_Delay(self, 26);

    for (size_t idx = 0; idx < len; idx++)
    {
        LIBOBD2_UART_SendByte(self, msg[idx]);
        LIBOBD2_TMR_Delay(self, P1_TIME_MIN);
    }

    // Clear echo
    LIBOBD2_UART_FlushRx(self);

    memset(&status, 0, sizeof(obd_status_t));
    return status;
}

OBD2_STATIC obd_status_t ECU_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
{
    uint8_t *pMsg       = (uint8_t *)recvdMsg;
    obd_status_t status = {0};
    uint8_t idx         = 0;

    status = ECU_ISO9141_ReadHeader(self, &recvdMsg->header);
    OBD2_ASSERT_OK(status);

    // Read data
    while(OBD_ERR_TIMEOUT_MAX != status.timeout)
    {
        status = DL_ReadByteInTimeframe(self, NULL, pMsg + idx + 4, P4_TIME_MIN, 20);
        idx++;
    }
    *len = idx;

    // Remove cs from data field
    recvdMsg->cs = pMsg[idx+2];
    pMsg[idx+2] = 0x0;

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC void ECU_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len, size_t dataLen)
{
    size_t idx = 0;

    aSentMsg[idx++] = sentMsg->header.fmt;
    aSentMsg[idx++] = sentMsg->header.trgAddr;
    aSentMsg[idx++] = sentMsg->header.srcAddr;
    aSentMsg[idx++] = sentMsg->data.sid;

    for (size_t i = 0; i < dataLen - 1; i++)
    {
        aSentMsg[idx++] = sentMsg->data.param[i];
    }

    aSentMsg[idx++] = sentMsg->cs;

    if (len != NULL)
        *len = idx;
}

OBD2_STATIC obd_status_t ECU_ISO9141_5BaudInit(dataLink_if_t *self)
{
    iso9141_ctx_t *ctx = (iso9141_ctx_t *)(self->pProtocolCtx);
    obd_status_t status = {0};
    (void)status;
    (void)ctx;
    uint8_t syncByte = 0;
    uint8_t kb2Inverted = 0;

    status.response = 0x255;
    // Read wake-up byte at 5 baudRate
    if(!DL_RecvByteBlocking(self, &syncByte)) // TODO: Fix bug here, it shall read bye at 5 baud rate
    {
        goto exit;
    }

    OBD2_ASSERT_EQUAL_OR_EXIT(0x33, syncByte);
    LIBOBD2_TMR_Delay(self, ISO9141_W1_TIME_MIN);

    // Send sync byte
    LIBOBD2_UART_SendByte(self, 0x55);
    LIBOBD2_UART_FlushRx(self);

    // Send KB1
    YIELD;
    LIBOBD2_TMR_Delay(self, ISO9141_W2_TIME_MIN);
    LIBOBD2_UART_SendByte(self, 0x08);
    LIBOBD2_UART_FlushRx(self);

    // Send KB2
    LIBOBD2_UART_SendByte(self, 0x08);
    LIBOBD2_UART_FlushRx(self);

    // Receive kb2 inverted
    status = DL_ReadByteInTimeframe(self, NULL, &kb2Inverted, 0, ISO9141_W4_TIME_MAX);
    OBD2_ASSERT_EQUAL_OR_EXIT((uint8_t)~0x08, kb2Inverted);

    // Send inverted address
    YIELD;
    LIBOBD2_TMR_Delay(self, ISO9141_W4_TIME_MIN);
    LIBOBD2_UART_SendByte(self, ~0x33);

    ctx->header.fmt = 0x48;
    ctx->header.trgAddr = 0x6B;
    ctx->header.srcAddr = 0x12;

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t ecu_iso9141_connect(dataLink_if_t *self, uint8_t* protocol)
{
    iso9141_ctx_t  *ctx = (iso9141_ctx_t *)(self->pProtocolCtx);
    memset(ctx, 0, sizeof(iso9141_ctx_t));
    *protocol = ISO9141;

    return ECU_ISO9141_5BaudInit(self);
}

obd_status_t ecu_iso9141_send_request(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t len)
{
    iso9141_ctx_t ctx = *(iso9141_ctx_t *)(self->pProtocolCtx);
    uint8_t aMessage[11] = {0};
    obd_status_t status;
    size_t msgLen = 0;
    message_t message = {0};

    obd_data_t data = *serviceRequests;

    // Construct the message
    message.header.fmt = ctx.header.fmt;
    message.header.trgAddr = ctx.header.trgAddr;
    message.header.srcAddr = ctx.header.srcAddr;

    message.data = data;
    message.cs = ECU_ISO9141_ComputeChecksum(message.header, data, len);

    ECU_ISO9141_PrepareMessage(&message, aMessage, &msgLen, len);

    status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    status = ECU_ISO9141_SendMessage(self, aMessage, msgLen);
    OBD2_ASSERT_OK(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

obd_status_t ecu_iso9141_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t *len)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
    status = ECU_ISO9141_RecvMessage(self, &recvdMsg, len);
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
