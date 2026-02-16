/* ================================================ INCLUDES =============================================== */
#include "l2_kwp.h"
#include "data_link_if.h"
#include "l2_iso9141_init.h"
#include "srv_status.h"
#include "timing_if.h"
#include "transport_if.h"
#include "utils.h"
#include <string.h>
#include "l2_kwp_utils.h"

/* ================================================= MACROS ================================================ */

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data);

static obd_status_t L2_KWP_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
static obd_status_t L2_KWP_RecvMessage(dataLink_if_t *self, message_t *recvdMsg);

#if defined(SPT_FAST_INIT)
static obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t *self);
#endif /* SPT_FAST_INIT */
#if 0
static obd_status_t L2_KWP_SRV_StopCommunication(dataLink_if_t* self);
#endif
#if defined(SPT_CHANGE_TIMING_PARAM)
static obd_status_t L2_KWP_SRV_AccessTimingParameter(dataLink_if_t *self);
#endif /* SPT_CHANGE_TIMING_PARAM */
#if defined(SPT_FAST_INIT)
static obd_status_t L2_KWP_SRV_SendData( dataLink_if_t *self, message_t *sentMsg, message_t *recvdMsg);
#endif

#if defined(SPT_5BAUD_INIT)
static obd_status_t L2_KWP_5BaudInit(dataLink_if_t *self);
#endif /* SPT_5BAUD_INIT */

#if defined(SPT_FAST_INIT)
static obd_status_t L2_KWP_FastInit(dataLink_if_t *self);
#endif /* SPT_FAST_INIT */
static obd_status_t L2_KWP_Init(dataLink_if_t *self);
#if defined(SPT_FAST_INIT)
static void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self);
#endif /* SPT_FAST_INIT */
static inline obd_status_t L2_KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data)
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

static inline obd_status_t L2_KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen)
{
    uint8_t *buffer = (uint8_t *)header;
    uint32_t p2TimeElapsed = 0;
    obd_status_t status;

    // Format byte
    LIBOBD_ReceiveByte(self, buffer);

    // Check for P2 Timeout from Tester to ECU
    OBD2_ASSERT_EQUAL_OR_ERR(false, LIBOBD_IsTimeoutExpired(self), OBD_ERR_COMM_P2_TIMEOUT_TESTER_ECU);
    p2TimeElapsed = LIBOBD_GetTimeMs(self);
    if (p2TimeElapsed < KWP_P2_TIME_MIN) return OBD_ERR_COMM_P2_TIMEOUT_TESTER_ECU ;

    // Target byte
    status = ReadByteInTimeframe(self, buffer + 1, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // Source byte
    status = ReadByteInTimeframe(self, buffer + 2, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    *headerLen = 3;

    if (header->fmt.bit.len == 0)
    {
        *headerLen = 4;

        // Length byte
        status = ReadByteInTimeframe(self, buffer + 3, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
        OBD2_ASSERT_OK(status);
    }

    return OBD_STATUS_OK;
}

static obd_status_t L2_KWP_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len)
{
    uint32_t p3TimeElapsed = 0;

    // Check for P3 Timeout from P2 end to Tester
    OBD2_ASSERT_EQUAL_OR_ERR(false, LIBOBD_IsTimeoutExpired(self), OBD_ERR_COMM_P3_TIMEOUT_ECU_TESTER);
    p3TimeElapsed = LIBOBD_GetTimeMs(self);
    if (p3TimeElapsed < KWP_P3_TIME_MIN) return OBD_ERR_COMM_P3_TIMEOUT_ECU_TESTER;

    for (size_t idx = 0; idx < len; idx++)
    {
        LIBOBD_SendByte(self, msg[idx]);
        LIBOBD_Delay(self, KWP_P4_TIME_MIN);
    }

    LIBOBD_StartTimeout(self, KWP_P2_TIME_MAX);

    return OBD_STATUS_OK;
}

static obd_status_t L2_KWP_RecvMessage(dataLink_if_t *self, message_t *recvdMsg)
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
    for (int idx = 0; idx < len; idx++)
    {
        status = ReadByteInTimeframe(self, pMsg + idx + headerLen + 1, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
        OBD2_ASSERT_OK(status);
    }

    // Read CS
    status = ReadByteInTimeframe(self, &recvdMsg->cs, KWP_P1_TIME_MIN, KWP_P1_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // P2 Timeout from ECU to ECU
    LIBOBD_StartTimeout(self, KWP_P2_TIME_MAX);

    return OBD_STATUS_OK;
}

#if defined(SPT_FAST_INIT)
static void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self)
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
#endif /* SPT_FAST_INIT */

void L2_KWP_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len)
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
    *len = idx;
}

/******************************************* ISO 14230-2 Services ********************************************/
#if defined(SPT_FAST_INIT)
static obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t *self)
{
    message_t recvdMsg = {0};
    obd_status_t status;
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)self->pProtocolCtx;

    // Actual StartCommunication
    {
        header_t hdr = {
            .fmt = {0x81},
            .trgAddr = 0x33,
            .srcAddr = 0xF1,
            .len = 0,
        };

        message_t sentMsg = {
            .header = hdr,
            .data = START_COMM_REQ,
            .cs = L2_KWP_ComputeChecksum(hdr, START_COMM_REQ),
        };

        status = L2_KWP_SRV_SendData(self, &sentMsg, &recvdMsg);
        OBD2_ASSERT_OK(status);
    }

    // Extension
    {
        obd_response_t *resp = &recvdMsg.data.resp;

        if (resp->negative.negResp != 0x7F)
        {
            uint8_t kb1 = resp->positive.data[0];
            uint8_t kb2 = resp->positive.data[1];

            ctx->kb1 = kb1;
            ctx->kb2 = kb2;
        }
    }

    return OBD_STATUS_OK;
}
#endif /* SPT_FAST_INIT */

#if 0
static obd_status_t L2_KWP_SRV_StopCommunication(dataLink_if_t* self)
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

#if defined(SPT_FAST_INIT)
static obd_status_t L2_KWP_SRV_SendData(dataLink_if_t *self, message_t *sentMsg, message_t *recvdMsg)
{
    uint8_t aSentMsg[256] = {0};
    size_t sentMsgLen = 0;
    obd_status_t status;

    L2_KWP_PrepareMessage(sentMsg, aSentMsg, &sentMsgLen);

    // Tester sends request
    status = L2_KWP_SendMessage(self, aSentMsg, sentMsgLen);
    OBD2_ASSERT_OK(status);

    // TODO: Handle cases where ecu sends multiple messages
    {
        status = L2_KWP_RecvMessage(self, recvdMsg);
        OBD2_ASSERT_OK(status);

        // P2 Timeout from ECU response to ECU response
        while(LIBOBD_IsTimeoutExpired(self));
        LIBOBD_StopTimeout(self);
    }

    // P3 delay min until tester can send next request
    LIBOBD_Delay(self, KWP_P3_TIME_MIN);

    return status;
}
#endif

/******************************************* ISO 14230-2 Services ********************************************/

/********************************************* ISO 14230-2 INIT **********************************************/
#if defined(SPT_5BAUD_INIT)
static obd_status_t L2_KWP_5BaudInit(dataLink_if_t *self)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)(self->pProtocolCtx);
    obd_status_t status;
    uint8_t syncByte = 0;
    uint8_t kb1 = 0;
    uint8_t kb2 = 0;
    uint8_t invAddr = 0;
    const uint8_t targetAddr = 0x33;

    LIBOBD_Delay(self, ISO9141_W5_TIME_MIN);

    // Send address byte at 5 baud rate
    SendByteBitBang(self, targetAddr, 5);

    // Read Sync byte
    status = ReadByteInTimeframe(self, &syncByte, ISO9141_W1_TIME_MIN, ISO9141_W1_TIME_MAX);
    OBD2_ASSERT_OK(status);
    OBD2_ASSERT_EQUAL_OR_ERR(0x55, syncByte, OBD_ERR_5BAUD_WRONG_SYNC_BYTE);

    // Receive KB1 (W2 timing: 5-20ms)
    status = ReadByteInTimeframe(self, &kb1, ISO9141_W2_TIME_MIN, ISO9141_W2_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // Receive KB2 (W3 timing: 0-20ms)
    status = ReadByteInTimeframe(self, &kb2, ISO9141_W3_TIME_MIN, ISO9141_W3_TIME_MAX);
    OBD2_ASSERT_OK(status);

    // Wait W4 (25-50ms) then send inverted KB2
    LIBOBD_Delay(self, ISO9141_W4_TIME_MIN);
    LIBOBD_SendByte(self, ~kb2);
    if (status != OBD_STATUS_OK)
        return OBD_GENERIC_ERROR;

    // Receive inverted address from ECU (W4 timing: 25-50ms)
    status = ReadByteInTimeframe(self, &invAddr, ISO9141_W4_TIME_MIN, ISO9141_W4_TIME_MAX);
    OBD2_ASSERT_OK(status);
    OBD2_ASSERT_EQUAL_OR_ERR((uint8_t)(~targetAddr), invAddr, OBD_ERR_5BAUD_WRONG_INV_ADDR);

    // Store keyword bytes for protocol identification
    ctx->kb1 = kb1;
    ctx->kb2 = kb2;

    return OBD_STATUS_OK;
}
#endif /* SPT_5BAUD_INIT */

#if defined(SPT_FAST_INIT)
static obd_status_t L2_KWP_FastInit(dataLink_if_t *self)
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
#endif /* SPT_FAST_INIT */

static obd_status_t L2_KWP_Init(dataLink_if_t *self)
{
    obd_status_t status;

    #if defined(SPT_FAST_INIT)
    status = L2_KWP_FastInit(self);
    OBD2_ASSERT_OK(status);
    #endif /* SPT_FAST_INIT */

#if defined(SPT_5BAUD_INIT)
    status = L2_KWP_5BaudInit(self);
    OBD2_ASSERT_OK(status);
#endif /* SPT_5BAUD_INIT */

    return OBD_STATUS_OK;
}

/********************************************* ISO 14230-2 INIT **********************************************/

/* ================================================ MODULE API ============================================= */
obd_status_t l2_kwp_connect(dataLink_if_t *self)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)(self->pProtocolCtx);
    obd_status_t status;

    status = LIBOBD_TimingInit(self);
    OBD2_ASSERT_OK(status);

    status = LIBOBD_TransportInit(self);
    OBD2_ASSERT_OK(status);

    status = L2_KWP_Init(self);
    OBD2_ASSERT_OK(status);

    ctx->conStatus.bits.CONN_OK = 1;

    return OBD_STATUS_OK;
}

obd_status_t l2_kwp_send_request(dataLink_if_t *self, const obd_request_t *req, size_t len)
{
    l2_kwp_ctx_t ctx = *(l2_kwp_ctx_t *)(self->pProtocolCtx);
    uint8_t aMessage[256] = {0};
    obd_status_t status;
    size_t msgLen = 0;

    data_t data = {.req = *req, .len = len};

    // Construct the message
    message_t message =
    {
        .header = ctx.header,
        .data = data,
        .cs = L2_KWP_ComputeChecksum(ctx.header, data)
    };

    L2_KWP_PrepareMessage(&message, aMessage, &msgLen);

    status = L2_KWP_SendMessage(self, aMessage, msgLen);
    OBD2_ASSERT_OK(status);

    return OBD_STATUS_OK;
}

obd_status_t l2_kwp_recv_response(dataLink_if_t *self, obd_response_t *resp, size_t *len)
{
    message_t recvdMsg = {0};
    obd_status_t status;

    status = L2_KWP_RecvMessage(self, &recvdMsg);
    OBD2_ASSERT_OK(status);

    *resp = recvdMsg.data.resp;
    *len = recvdMsg.data.len;

    if (resp->negative.negResp != 0x7F)
    {
        // do something
    }

    return OBD_STATUS_OK;
}
