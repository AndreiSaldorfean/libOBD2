/* ================================================ INCLUDES =============================================== */
#include "l2_kwp2000.h"
#include "datalink.h"
#include "statusRetCodes.h"
#include "l2_iso9141.h"
#include "timing_if.h"
#include "transport_if.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include "l2_kwp2000.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data);
OBD2_STATIC obd_status_t L2_KWP_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
OBD2_STATIC obd_status_t L2_KWP_RecvMessage(dataLink_if_t *self, message_t *recvdMsg);
#if 0
OBD2_STATIC obd_status_t L2_KWP_SRV_StopCommunication(dataLink_if_t* self);
#endif
OBD2_STATIC obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t *self);
OBD2_STATIC obd_status_t L2_KWP_FastInit(dataLink_if_t *self);
OBD2_STATIC void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self);
#if defined(SPT_CHANGE_TIMING_PARAM)
OBD2_STATIC obd_status_t L2_KWP_SRV_AccessTimingParameter(dataLink_if_t *self);
#endif /* SPT_CHANGE_TIMING_PARAM */
OBD2_STATIC OBD2_INLINE obd_status_t L2_KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen);
OBD2_STATIC void L2_KWP_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
OBD2_STATIC uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data)
{
    uint8_t *hdr = (uint8_t *)&header;
    uint8_t *req = (uint8_t *)&data;
    uint8_t checksum = 0;
    uint8_t headerLen = (header.len == 0) ? 3 : 4;

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

OBD2_STATIC OBD2_INLINE obd_status_t L2_KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen)
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

    *headerLen = 3;

    if (header->fmt.bit.len == 0)
    {
        *headerLen = 4;

        // Length byte
        status.response = OBD_ERR_COMM_LEN_BYTE_NOT_RECVD;
        status = ReadByteInTimeframe(self, buffer + 3, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
        OBD2_ASSERT_OK(status);
    }

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC obd_status_t L2_KWP_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len)
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

OBD2_STATIC obd_status_t L2_KWP_RecvMessage(dataLink_if_t *self, message_t *recvdMsg)
{
    uint8_t *pMsg = (uint8_t *)recvdMsg;
    obd_status_t status;
    size_t headerLen = 0;
    uint8_t len = 0;

    /* Read the header to get the length of data */
    status = L2_KWP_ReadHeader(self, &recvdMsg->header, &headerLen);
    OBD2_ASSERT_OK(status);

    // Use the length byte if it's not 0 else get the length from the format byte
    len = (recvdMsg->header.len == 0) ? recvdMsg->header.fmt.bit.len
                                      : recvdMsg->header.len;

    // Read data
    status.response = OBD_ERR_COMM_DATA_BYTE_NOT_RECVD;
    for (int idx = 0; idx < len; idx++)
    {
        status = ReadByteInTimeframe(self, pMsg + idx + headerLen + 1, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
        OBD2_ASSERT_OK(status);
    }

    // Read CS
    status.response = OBD_ERR_COMM_CS_BYTE_NOT_RECVD;
    status = ReadByteInTimeframe(self, &recvdMsg->cs, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // P2 Timeout from ECU to ECU
    LIBOBD_StartTimeout(self, KWP_P2_TIME_MAX);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

OBD2_STATIC void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)self->pProtocolCtx;

    if (!ctx->conStatus.bits.POR)
    {
        LIBOBD_Delay(self, 300);
        ctx->conStatus.bits.POR = 1;
    }
    else if (ctx->conStatus.bits.STOP_COMM)
    {
        LIBOBD_Delay(self, 55);
        ctx->conStatus.bits.STOP_COMM = 0;
    }
}

OBD2_STATIC void L2_KWP_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len)
{
    size_t headerLen = (sentMsg->header.len == 0) ? 3 : 4;
    // size_t dataLen = sentMsg->data.len;
    size_t idx = 0;

    aSentMsg[idx++] = sentMsg->header.fmt.val;
    aSentMsg[idx++] = sentMsg->header.trgAddr;
    aSentMsg[idx++] = sentMsg->header.srcAddr;
    if (headerLen == 4)
    {
        aSentMsg[idx++] = sentMsg->header.len;
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

/******************************************* ISO 14230-2 Services ********************************************/
OBD2_STATIC obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t *self)
{
    (void) self;
    // message_t recvdMsg = {0};
    obd_status_t status = {0};
    // l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)self->pProtocolCtx;
    // uint8_t aMessage[256] = {0};
    // size_t msgLen = 0;
    //
    // data_t data = {.req = *req, .len = len};
    //
    // // Construct the message
    // message_t message = {0};
    // message.header.small.fmt.val = 0x68;
    // message.header.small.trgAddr = 0x6a;
    // message.header.small.srcAddr = 0xf1;
    // // message.header.small.fmt.bit.len = len + 1;
    // message.data = data;
    // message.cs = L2_KWP_ComputeChecksum(message.header, data);
    //
    // L2_KWP_PrepareMessage(&message, aMessage, &msgLen);
    //
    // status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    // status = L2_KWP_SendMessage(self, aMessage, msgLen);
    //
    // // Actual StartCommunication
    // {
    //     header_t hdr = {
    //         .small =
    //         {
    //             .fmt = {0x81},
    //             .trgAddr = 0x33,
    //             .srcAddr = 0xF1,
    //         }
    //     };
    //
    //     message_t sentMsg = {
    //         .header = hdr,
    //         .data = START_COMM_REQ,
    //         .cs = L2_KWP_ComputeChecksum(hdr, START_COMM_REQ),
    //     };
    //
    //     L2_KWP_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len)
    //     status = L2_KWP_SendMessage(self, &sentMsg, &recvdMsg);
    //     OBD2_ASSERT_OK(status);
    // }

    // Extension
//     {
//         obd_response_t *resp = &recvdMsg.data.resp;
//
//         if (resp->negative.negResp != 0x7F)
//         {
//             uint8_t kb1 = resp->positive.data[0];
//             uint8_t kb2 = resp->positive.data[1];
//
//             ctx->kb1 = kb1;
//             ctx->kb2 = kb2;
//         }
//     }
//
// exit:
    return status;
}

#if 0
OBD2_STATIC obd_status_t L2_KWP_SRV_StopCommunication(dataLink_if_t* self)
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
            // .cs = L2_KWP_ComputeChecksum(),
        };

        status = L2_KWP_SRV_SendData(self, &sentMsg, &recvdMsg);
        OBD2_ASSERT_OK(status);
    }

    return OBD_STATUS_OK;
}
#endif
/******************************************* ISO 14230-2 Services ********************************************/

OBD2_STATIC obd_status_t L2_KWP_FastInit(dataLink_if_t *self)
{
    obd_status_t status;

    L2_KWP_IdleBasedOnConnStatus(self);

    LIBOBD_SwitchMode(self, FAST_INIT_WAKEUP_START);

    // ISO 14230 Fast Init: LOW for 25ms (TiniL)
    LIBOBD_SendPulse(self, PULSE_LOW);
    LIBOBD_Delay(self, 25);

    // ISO 14230 Fast Init: HIGH for 25ms (TiniH)
    LIBOBD_SendPulse(self, PULSE_HIGH);
    LIBOBD_Delay(self, 25);

    LIBOBD_SwitchMode(self, FAST_INIT_WAKEUP_END);

    // For keeping the size of the function small the L2_KWP_SRV_StartCommunication
    // will do more than just sending the service
    status = L2_KWP_SRV_StartCommunication(self);

    return status;
}

/* ================================================ MODULE API ============================================= */
obd_status_t l2_kwp_connect(dataLink_if_t *self, uint8_t* protocol)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)(self->pProtocolCtx);
    obd_status_t status = {0};
    memset(ctx, 0, sizeof(l2_kwp_ctx_t));

    status.response = OBD_ERR_CONN_TIMER_INIT_FAILED;
    if(!LIBOBD_TimingInit(self)) goto exit;
    status.response = OBD_ERR_CONN_TRANS_INIT_FAILED;
    if(!LIBOBD_TransportInit(self)) goto exit;

    status = L2_KWP_FastInit(self);
    OBD2_ASSERT_OK(status);

    ctx->conStatus.bits.CONN_OK = 1;
    *protocol = KWP2000;

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

obd_status_t l2_kwp_send_request(dataLink_if_t *self, const obd_request_t *req, size_t len)
{
    l2_kwp_ctx_t ctx = *(l2_kwp_ctx_t *)(self->pProtocolCtx);
    uint8_t aMessage[256] = {0};
    obd_status_t status;
    size_t msgLen = 0;

    data_t data = {.req = *req, .len = len};

    // Construct the message
    message_t message = {0};
    message.header = ctx.header;
    message.header.fmt.bit.len = len + 1;
    message.data = data;
    message.cs = L2_KWP_ComputeChecksum(message.header, data);

    L2_KWP_PrepareMessage(&message, aMessage, &msgLen);

    status.response = OBD_ERR_COMM_SEND_MSG_FAILED;
    status = L2_KWP_SendMessage(self, aMessage, msgLen);
    OBD2_ASSERT_OK(status);

    // Clear echo
    LIBOBD_FlushRx(self);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

obd_status_t l2_kwp_recv_response(dataLink_if_t *self, obd_response_t *resp, size_t *len)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    status.response = OBD_ERR_COMM_RECV_MSG_FAILED;
    status = L2_KWP_RecvMessage(self, &recvdMsg);
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
