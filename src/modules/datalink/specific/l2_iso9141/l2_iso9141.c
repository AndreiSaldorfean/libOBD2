/* ================================================ INCLUDES =============================================== */
#include "l2_kwp2000.h"
#include "datalink.h"
#include "statusRetCodes.h"
#include "l2_iso9141.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include "l2_kwp2000.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC uint8_t L2_ISO9141_ComputeChecksum(header_t header, data_t data);
OBD2_STATIC obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg);
OBD2_STATIC OBD2_INLINE obd_status_t L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header);
OBD2_STATIC void L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
OBD2_STATIC uint8_t L2_ISO9141_ComputeChecksum(header_t header, data_t data)
{
    uint8_t *hdr = (uint8_t *)&header;
    uint8_t *req = (uint8_t *)&data;
    uint8_t checksum = 0;
    uint8_t headerLen = (header.big.len == 0) ? 3 : 4;

    for (uint8_t idx = 0; idx < headerLen; idx++)
    {
        checksum += hdr[idx];
    }

    for (uint8_t idx = 0; idx < data.len + 1; idx++)
    {
        checksum += req[idx];
    }

    return checksum % 256;
}

OBD2_STATIC OBD2_INLINE obd_status_t L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header)
{
    uint8_t *buffer = (uint8_t *)header;
    uint32_t p2TimeElapsed = 0;
    obd_status_t status = {0};

    // Format byte
    while (!LIBOBD_ReceiveByte(self, buffer))
    {
        // Check for P2 Timeout from Tester to ECU
        status.response = OBD_ERR_COMM_FMT_BYTE_NOT_RECVD;
        status.timeout = OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU;
        OBD2_ASSERT_EQUAL_OR_EXIT(false, LIBOBD_IsTimeoutExpired(self));
    }

    p2TimeElapsed = LIBOBD_GetTimeMs(self);
    p2TimeElapsed -= LIBOBD_GetTimeSample(self);

    status.timeout = OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU;
    OBD2_IF_COND_GOTO_EXIT(p2TimeElapsed < KWP_P2_TIME_MIN);

    // Target byte
    status.response = OBD_ERR_COMM_TRGT_BYTE_NOT_RECVD;
    status = ReadByteInTimeframe(self, buffer + 1, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // Source byte
    status.response = OBD_ERR_COMM_SRC_BYTE_NOT_RECVD;
    status = ReadByteInTimeframe(self, buffer + 2, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    uint32_t timingSample  = 0;
    obd_status_t status    = {0};

    LIBOBD_Delay(self, KWP_P3_TIME_MIN);

    for (size_t idx = 0; idx < len; idx++)
    {
        LIBOBD_SendByte(self, msg[idx]);
        LIBOBD_Delay(self, KWP_P4_TIME_MIN);
    }

    timingSample = LIBOBD_GetTimeMs(self);
    LIBOBD_SetTimeSample(self, timingSample);
    LIBOBD_StartTimeout(self, KWP_P2_TIME_MAX);

    // Clear echo
    LIBOBD_FlushRx(self);

    memset(&status, 0, sizeof(obd_status_t));
    return status;
}

OBD2_STATIC obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg)
{
    uint8_t *pMsg = (uint8_t *)recvdMsg;
    obd_status_t status = {0};
    uint8_t idx = 0;

    /* Read the header to get the length of data */
    status = L2_ISO9141_ReadHeader(self, &recvdMsg->header);
    OBD2_ASSERT_OK(status);

    // Read data
    while(OBD_ERR_TIMEOUT_MAX == status.timeout)
    {
        status = ReadByteInTimeframe(self, pMsg + idx + 4, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
        idx++;
    }

    // Remove cs from data field
    recvdMsg->cs = pMsg[idx+3];
    pMsg[idx+3] = 0x0;

    // P2 Timeout from ECU to ECU
    LIBOBD_StartTimeout(self, KWP_P2_TIME_MAX);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC void L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len)
{
    size_t headerLen = (sentMsg->header.big.len == 0) ? 3 : 4;
    // size_t dataLen = sentMsg->data.len;
    size_t idx = 0;

    aSentMsg[idx++] = sentMsg->header.small.fmt.val;
    aSentMsg[idx++] = sentMsg->header.small.trgAddr;
    aSentMsg[idx++] = sentMsg->header.small.srcAddr;
    if (headerLen == 4)
    {
        aSentMsg[idx++] = sentMsg->header.big.len;
    }

    aSentMsg[idx++] = sentMsg->data.req.sid;

    for (uint8_t i = 0; i < sentMsg->data.len; i++)
    {
        aSentMsg[idx++] = sentMsg->data.req.param[i];
    }

    aSentMsg[idx++] = sentMsg->cs;

    if (len != NULL)
        *len = idx;
}

OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit(dataLink_if_t *self, uint8_t* protocol)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)(self->pProtocolCtx);
    obd_status_t status;
    (void)status;
    uint8_t syncByte = 0;
    uint8_t kb1 = 0;
    uint8_t kb2 = 0;
    uint8_t invAddr = 0;
    const uint8_t targetAddr = 0x33;

    LIBOBD_Delay(self, ISO9141_W5_TIME_MIN);

    // Send address byte at 5 baud rate
    SendByteBitBang(self, targetAddr, 5);

    // Read Sync byte
    status.response = OBD_ERR_5BAUD_SYNC_NOT_RECVD;
    status = ReadByteInTimeframe(self, &syncByte, ISO9141_W1_TIME_MIN, ISO9141_W1_TIME_MAX);
    OBD2_ASSERT_OK(status);
    status.response = OBD_ERR_5BAUD_WRONG_SYNC_BYTE;
    OBD2_ASSERT_EQUAL_OR_EXIT(0x55, syncByte);

    // Receive KB1 (W2 timing: 5-20ms)
    status.response = OBD_ERR_5BAUD_KB1_NOT_RECVD;
    status = ReadByteInTimeframe(self, &kb1, ISO9141_W2_TIME_MIN, ISO9141_W2_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // Receive KB2 (W3 timing: 0-20ms)
    status.response = OBD_ERR_5BAUD_KB2_NOT_RECVD;
    status = ReadByteInTimeframe(self, &kb2, ISO9141_W3_TIME_MIN, ISO9141_W3_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // Wait W4 (25-50ms) then send inverted KB2
    LIBOBD_Delay(self, ISO9141_W4_TIME_MIN);
    LIBOBD_SendByte(self, ~kb2);

    // Clear echo
    LIBOBD_FlushRx(self);

    // Receive inverted address from ECU (W4 timing: 25-50ms)
    status.response = OBD_ERR_5BAUD_INV_ADDR_NOT_RECVD;
    status = ReadByteInTimeframe(self, &invAddr, ISO9141_W4_TIME_MIN, ISO9141_W4_TIME_MAX);
    OBD2_ASSERT_OK(status);
    status.response = OBD_ERR_5BAUD_WRONG_INV_ADDR;
    OBD2_ASSERT_EQUAL_OR_EXIT((uint8_t)(~targetAddr), invAddr);

    // Store keyword bytes for protocol identification
    ctx->kb1 = kb1;
    ctx->kb2 = kb2;

    // ISO9141-2
    // If 0x08 then 0x68 0x6A 0xF1 header is used always with 7 bytes of data max
    if (kb1 == 0x08 && kb2 == 0x08)
    {
        // Request
        ctx->header.small.fmt.val = 0x68;

        // ECU
        ctx->header.small.trgAddr = 0x6A;

        // Tester
        ctx->header.small.srcAddr = 0xF1;

        *protocol = ISO9141;
    }
    // Can use KWP2000
    else if (kb1 == 0x94 && kb2 == 0x94)
    {
        // Functional addr
        // Can use kwp2000 header and len
        ctx->header.small.fmt.bit.a0 = 1;
        ctx->header.small.fmt.bit.a1 = 1;

        // ECU
        ctx->header.small.trgAddr = 0x33;

        // Tester
        ctx->header.small.srcAddr = 0xF1;

        *protocol = KWP2000;
    }
    // Invalid keybytes
    else
    {
        status.response = OBD_ERR_5BAUD_WRONG_KEYBYTES;
    }

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t l2_iso9141_connect(dataLink_if_t *self, uint8_t* protocol)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)(self->pProtocolCtx);
    memset(ctx, 0, sizeof(l2_kwp_ctx_t));

    return L2_ISO9141_5BaudInit(self, protocol);
}

obd_status_t l2_iso9141_send_request(dataLink_if_t *self, const obd_request_t *req, size_t len)
{
    l2_kwp_ctx_t ctx = *(l2_kwp_ctx_t *)(self->pProtocolCtx);
    uint8_t aMessage[256] = {0};
    obd_status_t status;
    size_t msgLen = 0;
    message_t message = {0};

    data_t data = {.req = *req, .len = len};

    // Construct the message
    message.header.small.fmt = ctx.header.small.fmt;
    message.header.small.trgAddr = ctx.header.small.trgAddr;
    message.header.small.srcAddr = ctx.header.small.srcAddr;

    message.data = data;
    message.cs = L2_ISO9141_ComputeChecksum(message.header, data);

    L2_ISO9141_PrepareMessage(&message, aMessage, &msgLen);

    status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    status = L2_ISO9141_SendMessage(self, aMessage, msgLen);
    OBD2_ASSERT_OK(status);

    // Clear echo
    LIBOBD_FlushRx(self);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

obd_status_t l2_iso9141_recv_response(dataLink_if_t *self, obd_response_t *resp, size_t *len)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
    status = L2_ISO9141_RecvMessage(self, &recvdMsg);
    OBD2_ASSERT_OK(status);

    *resp = recvdMsg.data.resp;
    *len = recvdMsg.data.len;

    if (resp->negative.negResp != 0x7F)
    {
        // do something
    }

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}
