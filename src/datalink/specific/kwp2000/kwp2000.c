/* ================================================ INCLUDES =============================================== */
#include "kwp2000.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include "kwp2000.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC uint8_t KWP_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen);
OBD2_STATIC obd_status_t KWP_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t KWP_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len);
#if 0
OBD2_STATIC obd_status_t KWP_SRV_StopCommunication(dataLink_if_t* self);
#endif
OBD2_STATIC obd_status_t KWP_SRV_StartCommunication(dataLink_if_t *self);
OBD2_STATIC obd_status_t KWP_FastInit(dataLink_if_t *self);
OBD2_STATIC void KWP_IdleBasedOnConnStatus(dataLink_if_t *self);
#if defined(SPT_CHANGE_TIMING_PARAM)
OBD2_STATIC obd_status_t KWP_SRV_AccessTimingParameter(dataLink_if_t *self);
#endif /* SPT_CHANGE_TIMING_PARAM */
OBD2_STATIC OBD2_INLINE obd_status_t KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen);
OBD2_STATIC void KWP2000_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
OBD2_STATIC uint8_t KWP_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen)
{
    uint8_t *hdr = (uint8_t *)&header;
    uint8_t *serviceRequests = (uint8_t *)&data;
    uint8_t checksum = 0;
    uint8_t headerLen = (header.len == 0) ? 3 : 4;

    for (uint8_t idx = 0; idx < headerLen; idx++)
    {
        checksum += hdr[idx];
    }

    for (uint8_t idx = 0; idx < dataLen; idx++)
    {
        checksum += serviceRequests[idx];
    }

    return checksum % 256;
}

OBD2_STATIC OBD2_INLINE obd_status_t KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen)
{
    uint8_t *buffer = (uint8_t *)header;
    uint32_t p2TimeElapsed = 0;
    obd_status_t status = {0};
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_NO_DATA;

    // Format byte
    rxStatus = LIBOBD2_UART_ReceiveByte(self, buffer);
    while (OBD_UART_RX_NO_DATA == rxStatus)
    {
        // Check for P2 Timeout from Tester to ECU
        status.response = OBD_ERR_COMM_FMT_BYTE_NOT_RECVD;
        status.timeout = OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU;
        OBD2_ASSERT_EQUAL_OR_EXIT(false, LIBOBD2_TMR_IsTimeoutExpired(self));
        rxStatus = LIBOBD2_UART_ReceiveByte(self, buffer);
    }
    status.timeout = OBD_STATUS_OK;

    // The format byte arrived but was corrupted on the line
    status.response = DL_UartRxStatusToResponse(rxStatus);
    OBD2_IF_COND_GOTO_EXIT(OBD_STATUS_OK != status.response);

    p2TimeElapsed = LIBOBD2_TMR_GetTimeMs(self);
    p2TimeElapsed -= LIBOBD2_TMR_GetTimeSample(self);

    status.timeout = OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU;
    OBD2_IF_COND_GOTO_EXIT(p2TimeElapsed < P2_TIME_MIN);

    // Target byte
    status.response = OBD_ERR_COMM_TRGT_BYTE_NOT_RECVD;
    status = DL_ReadByteInTimeframe(self, NULL, buffer + 1, P1_TIME_MIN, P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // Source byte
    status.response = OBD_ERR_COMM_SRC_BYTE_NOT_RECVD;
    status = DL_ReadByteInTimeframe(self, NULL, buffer + 2, P1_TIME_MIN, P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    *headerLen = 3;

    if ((header->fmt & 0x3F) == 0)
    {
        *headerLen = 4;

        // Length byte
        status.response = OBD_ERR_COMM_LEN_BYTE_NOT_RECVD;
        status = DL_ReadByteInTimeframe(self, NULL, buffer + 3, P1_TIME_MIN, P1_TIME_MAX);
        OBD2_ASSERT_OK(status);
    }

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC obd_status_t KWP_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    uint32_t timingSample  = 0;
    obd_status_t status    = {0};

    LIBOBD2_TMR_Delay(self, P3_TIME_MIN);

    for (size_t idx = 0; idx < len; idx++)
    {
        LIBOBD2_UART_SendByte(self, msg[idx]);
        LIBOBD2_TMR_Delay(self, P4_TIME_MIN);
    }

    timingSample = LIBOBD2_TMR_GetTimeMs(self);
    LIBOBD2_TMR_SetTimeSample(self, timingSample);
    LIBOBD2_TMR_StartTimeout(self, P2_TIME_MAX);

    // Clear echo
    LIBOBD2_UART_FlushRx(self);

    memset(&status, 0, sizeof(obd_status_t));
    return status;
}

OBD2_STATIC obd_status_t KWP_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t* len)
{
    uint8_t *pMsg = (uint8_t *)recvdMsg;
    obd_status_t status;
    size_t headerLen = 0;
    uint8_t dataLen = 0;

    /* Read the header to get the length of data */
    status = KWP_ReadHeader(self, &recvdMsg->header, &headerLen);
    OBD2_ASSERT_OK(status);

    // Use the length byte if it's not 0 else get the length from the format byte
    dataLen = (recvdMsg->header.len == 0) ? (recvdMsg->header.fmt & 0x3F)
                                      : recvdMsg->header.len;

    // Read data
    status.response = OBD_ERR_COMM_DATA_BYTE_NOT_RECVD;
    for (int idx = 0; idx < dataLen; idx++)
    {
        status = DL_ReadByteInTimeframe(self, NULL, pMsg + idx + headerLen + 1, P1_TIME_MIN, P1_TIME_MAX);
        OBD2_ASSERT_OK(status);
    }
    *len = dataLen;

    // Read CS
    status.response = OBD_ERR_COMM_CS_BYTE_NOT_RECVD;
    status = DL_ReadByteInTimeframe(self, NULL, &recvdMsg->cs, P1_TIME_MIN, P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // P2 Timeout from ECU to ECU
    LIBOBD2_TMR_StartTimeout(self, P2_TIME_MAX);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC void KWP_IdleBasedOnConnStatus(dataLink_if_t *self)
{
    kwp_ctx_t *ctx = (kwp_ctx_t *)self->pProtocolCtx;

    if (!ctx->conStatus.bits.POR)
    {
        LIBOBD2_TMR_Delay(self, 300);
        ctx->conStatus.bits.POR = 1;
    }
    else if (ctx->conStatus.bits.STOP_COMM)
    {
        LIBOBD2_TMR_Delay(self, 55);
        ctx->conStatus.bits.STOP_COMM = 0;
    }
}

OBD2_STATIC void KWP2000_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len)
{
    size_t headerLen = (sentMsg->header.len == 0) ? 3 : 4;
    size_t idx = 0;

    aSentMsg[idx++] = sentMsg->header.fmt;
    aSentMsg[idx++] = sentMsg->header.trgAddr;
    aSentMsg[idx++] = sentMsg->header.srcAddr;
    if (headerLen == 4)
    {
        aSentMsg[idx++] = sentMsg->header.len;
    }

    aSentMsg[idx++] = sentMsg->data.sid;

    for (uint8_t i = 0; i < dataLen; i++)
    {
        aSentMsg[idx++] = sentMsg->data.param[i];
    }

    aSentMsg[idx++] = sentMsg->cs;

    if (len != NULL)
        *len = idx;
}

/******************************************* ISO 14230-2 Services ********************************************/
OBD2_STATIC obd_status_t KWP_SRV_StartCommunication(dataLink_if_t *self)
{
    (void) self;
    // message_t recvdMsg = {0};
    obd_status_t status = {0};
    #if 0 /* TODO: Fix logic and addapt to new library structure */
    kwp_ctx_t *ctx = (kwp_ctx_t *)self->pProtocolCtx;
    uint8_t aMessage[256] = {0};
    size_t msgLen = 0;

    obd_data_t data = {.serviceRequests = *serviceRequests, .len = len};

    // Construct the message
    message_t message = {0};
    message.header.small.fmt.val = 0x68;
    message.header.small.trgAddr = 0x6a;
    message.header.small.srcAddr = 0xf1;
    // message.header.small.fmt.bit.len = len + 1;
    message.data = data;
    message.cs = KWP_ComputeChecksum(message.header, data);

    KWP2000_PrepareMessage(&message, aMessage, &msgLen);

    status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    status = KWP_SendMessage(self, aMessage, msgLen);

    // Actual StartCommunication
    {
        header_t hdr = {
            .small =
            {
                .fmt = {0x81},
                .trgAddr = 0x33,
                .srcAddr = 0xF1,
            }
        };

        message_t sentMsg = {
            .header = hdr,
            .data = START_COMM_REQ,
            .cs = KWP_ComputeChecksum(hdr, START_COMM_REQ),
        };

        KWP2000_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len)
        status = KWP_SendMessage(self, &sentMsg, &recvdMsg);
        OBD2_ASSERT_OK(status);
    }

    // Extension
    {
        obd_data_t *resp = &recvdMsg.data.resp;

        if (resp->negative.negResp != 0x7F)
        {
            uint8_t kb1 = resp->positive.data[0];
            uint8_t kb2 = resp->positive.data[1];

            ctx->kb1 = kb1;
            ctx->kb2 = kb2;
        }
    }

exit:
    #endif

    return status;
}

#if 0
OBD2_STATIC obd_status_t KWP_SRV_StopCommunication(dataLink_if_t* self)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    // Actual StartCommunication
    {
        header_t hdr =
        {
            .fmt     = {0},
            .trgAddr = 0,
            .srcAddr = 0,
            .len     = 0,
        };

        message_t sentMsg =
        {
            .header = hdr,
            // .data = ,
            // .cs = KWP_ComputeChecksum(),
        };

        status = KWP_SRV_SendData(self, &sentMsg, &recvdMsg);
        OBD2_ASSERT_OK(status);
    }

    return OBD_STATUS_OK;
}
#endif
/******************************************* ISO 14230-2 Services ********************************************/

OBD2_STATIC obd_status_t KWP_FastInit(dataLink_if_t *self)
{
    obd_status_t status;

    KWP_IdleBasedOnConnStatus(self);

    LIBOBD2_UART_SwitchMode(self, FAST_INIT_WAKEUP_START);

    // ISO 14230 Fast Init: LOW for 25ms (TiniL)
    LIBOBD2_UART_SendPulse(self, PULSE_LOW);
    LIBOBD2_TMR_Delay(self, 25);

    // ISO 14230 Fast Init: HIGH for 25ms (TiniH)
    LIBOBD2_UART_SendPulse(self, PULSE_HIGH);
    LIBOBD2_TMR_Delay(self, 25);

    LIBOBD2_UART_SwitchMode(self, FAST_INIT_WAKEUP_END);

    // For keeping the size of the function small the KWP_SRV_StartCommunication
    // will do more than just sending the service
    status = KWP_SRV_StartCommunication(self);

    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t KWP_Connect(dataLink_if_t *self, uint8_t* protocol)
{
    kwp_ctx_t *ctx = (kwp_ctx_t *)(self->pProtocolCtx);
    obd_status_t status = {0};
    memset(ctx, 0, sizeof(kwp_ctx_t));

    status.response = OBD_ERR_CONN_TIMER_INIT_FAILED;
    if(OBD_TMR_OK != LIBOBD2_TMR_Init(self)) goto exit;
    status.response = OBD_ERR_CONN_TRANS_INIT_FAILED;
    if(!LIBOBD2_UART_Init(self)) goto exit;

    status = KWP_FastInit(self);
    OBD2_ASSERT_OK(status);

    ctx->conStatus.bits.CONN_OK = 1;
    *protocol = KWP2000;

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

obd_status_t KWP_SendRequest(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    kwp_ctx_t ctx = *(kwp_ctx_t *)(self->pProtocolCtx);
    uint8_t aSentMsg[256] = {0};
    obd_status_t status;
    size_t len = 0;

    obd_data_t data = *serviceRequests;

    // Construct the message
    message_t msg = {0};
    msg.header = ctx.header;
    msg.header.fmt = dataLen;
    msg.data = data;
    msg.cs = KWP_ComputeChecksum(msg.header, data, dataLen);

    KWP2000_PrepareMessage(&msg, aSentMsg, dataLen, &len);

    status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    status = KWP_SendMessage(self, aSentMsg, len);
    OBD2_ASSERT_OK(status);

    // Clear echo
    LIBOBD2_UART_FlushRx(self);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

obd_status_t KWP_RecvResponse(dataLink_if_t *self, obd_data_t *resp, size_t *dataLen)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
    status = KWP_RecvMessage(self, &recvdMsg, dataLen);
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
