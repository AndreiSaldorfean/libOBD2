/* ================================================ INCLUDES =============================================== */
#include "l2_kwp.h"
#include "data_link_if.h"
#include "l2_kwp_msg.h"
#include "l2_kwp_srv.h"
#include "l2_kwp_timing.h"
#include "srv_status.h"
#include "timing_if.h"
#include "transport_if.h"
#include "utils.h"
#include <string.h>

/* ================================================= MACROS ================================================ */
#define PULSE_HIGH (1U)
#define PULSE_LOW (0U)

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data);

static obd_status_t L2_KWP_SendMessage(
    void *timingHandle,
    const obd_timing_ops_t *pTimingOps,
    void *handle,
    const obd_transport_ops_t *transportOps,
    uint8_t *msg,
    size_t len);
static obd_status_t L2_KWP_RecvMessage(
    void *timingHandle,
    const obd_timing_ops_t *pTimingOps,
    void *handle,
    const obd_transport_ops_t *transportOps,
    message_t *recvdMsg);

static obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t *self);
#if 0
static obd_status_t L2_KWP_SRV_StopCommunication(dataLink_if_t* self);
#endif
#if defined(SPT_CHANGE_TIMING_PARAM)
static obd_status_t L2_KWP_SRV_AccessTimingParameter(dataLink_if_t *self);
#endif /* SPT_CHANGE_TIMING_PARAM */
static obd_status_t L2_KWP_SRV_SendData(
    dataLink_if_t *self,
    message_t *sentMsg,
    message_t *recvdMsg);

#if defined(SPT_5BUAD_INIT)
static obd_status_t L2_KWP_5BaudInit(dataLink_if_t *self);
#endif /* SPT_5BAUD_INIT */
static obd_status_t L2_KWP_FastInit(dataLink_if_t *self);
static obd_status_t L2_KWP_Init(dataLink_if_t *self);
static void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self);
static inline obd_status_t L2_KWP_ReadHeader(
    void *timingHandle,
    const obd_timing_ops_t *pTimingOps,
    void *handle,
    const obd_transport_ops_t *transportOps,
    header_t *header,
    size_t *headerLen);

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

static inline obd_status_t L2_KWP_ReadHeader(
    void *timingHandle,
    const obd_timing_ops_t *pTimingOps,
    void *handle,
    const obd_transport_ops_t *transportOps,
    header_t *header,
    size_t *headerLen)
{
    uint8_t *buffer = (uint8_t *)header;
    bool (*IsTimeoutExpired)(void *pHandle) = pTimingOps->is_timeout_expired;
    uint32_t (*GetTimeMs)(void *pHandle) = pTimingOps->get_time_ms;
    uint32_t p2TimeElapsed = 0;
    obd_status_t status;

    // Format byte
    status = transportOps->recv_byte(handle, buffer);

    // P2 Timeout from Tester to ECU
    OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
    p2TimeElapsed = GetTimeMs(timingHandle);
    if (p2TimeElapsed < KWP_P2_TIME_MIN)
    {
        return OBD_GENERIC_ERROR;
    }

    // Target byte
    pTimingOps->start_timeout(timingHandle, KWP_P1_TIME_MAX, NULL, NULL);
    status = transportOps->recv_byte(handle, buffer + 1);
    OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
    OBD2_ASSERT_OK(status);
    pTimingOps->stop_timeout(timingHandle);

    // Source byte
    pTimingOps->start_timeout(timingHandle, KWP_P1_TIME_MAX, NULL, NULL);
    status = transportOps->recv_byte(handle, buffer + 2);
    OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
    OBD2_ASSERT_OK(status);
    pTimingOps->stop_timeout(timingHandle);

    *headerLen = 3;

    if (header->fmt.bit.len == 0)
    {
        *headerLen = 4;

        // Length byte
        pTimingOps->start_timeout(timingHandle, KWP_P1_TIME_MAX, NULL, NULL);
        status = transportOps->recv_byte(handle, buffer + 3);
        OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
        OBD2_ASSERT_OK(status);
        pTimingOps->stop_timeout(timingHandle);
    }

    return OBD_STATUS_OK;
}

static obd_status_t L2_KWP_SendMessage(
    void *timingHandle,
    const obd_timing_ops_t *pTimingOps,
    void *handle,
    const obd_transport_ops_t *transportOps,
    uint8_t *msg,
    size_t len)
{
    bool (*IsTimeoutExpired)(void *pHandle) = pTimingOps->is_timeout_expired;
    uint32_t (*GetTimeMs)(void *pHandle) = pTimingOps->get_time_ms;
    obd_status_t status;
    uint32_t p3TimeElapsed = 0;

    OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
    p3TimeElapsed = GetTimeMs(timingHandle);
    pTimingOps->stop_timeout(timingHandle);
    if (p3TimeElapsed < KWP_P3_TIME_MIN)
    {
        return OBD_GENERIC_ERROR;
    }

    for (size_t idx = 0; idx < len; idx++)
    {
        status = transportOps->send_byte(handle, msg[idx]);
        OBD2_ASSERT_OK(status);
        pTimingOps->delay_ms(timingHandle, KWP_P4_TIME_MIN);
    }

    pTimingOps->start_timeout(timingHandle, KWP_P2_TIME_MAX, NULL, NULL);

    return OBD_STATUS_OK;
}

static obd_status_t L2_KWP_RecvMessage(
    void *timingHandle,
    const obd_timing_ops_t *pTimingOps,
    void *handle,
    const obd_transport_ops_t *transportOps,
    message_t *recvdMsg)
{
    uint8_t *pMsg = (uint8_t *)recvdMsg;
    bool (*IsTimeoutExpired)(void *pHandle) = pTimingOps->is_timeout_expired;
    obd_status_t status;
    uint8_t len = 0;
    size_t headerLen = 0;


    /* Read the header to get the length of data */
    L2_KWP_ReadHeader(
        timingHandle,
        pTimingOps,
        handle,
        transportOps,
        &recvdMsg->header,
        &headerLen);

    // Use the length byte if it's not 0 else get the length from the format byte
    len = (recvdMsg->header.len == 0) ? recvdMsg->header.fmt.bit.len
                                      : recvdMsg->header.len;

    // Read data
    for (int idx = 0; idx < len; idx++)
    {
        pTimingOps->start_timeout(timingHandle, KWP_P1_TIME_MAX, NULL, NULL);
        status = transportOps->recv_byte(handle, pMsg + idx + headerLen + 1);
        OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
        OBD2_ASSERT_OK(status);
        pTimingOps->stop_timeout(timingHandle);
    }

    // Read CS
    pTimingOps->start_timeout(timingHandle, KWP_P1_TIME_MAX, NULL, NULL);
    status = transportOps->recv_byte(handle, &recvdMsg->cs);
    OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
    OBD2_ASSERT_OK(status);
    pTimingOps->stop_timeout(timingHandle);

    // P2 Timeout from ECU to ECU
    pTimingOps->start_timeout(timingHandle, KWP_P2_TIME_MAX, NULL, NULL);

    return OBD_STATUS_OK;
}

static void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)self->pProtocolCtx;
    void *timingHandle = self->pTimingHandle;
    const obd_timing_ops_t *timingOps = self->pTimingOps;

    if (!ctx->conStatus.bits.POR)
    {
        timingOps->delay_ms(timingHandle, 300);
        ctx->conStatus.bits.POR = 1;
    }
    else if (ctx->conStatus.bits.STOP_COMM)
    {
        timingOps->delay_ms(timingHandle, 55);
        ctx->conStatus.bits.STOP_COMM = 0;
    }
}

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

static obd_status_t L2_KWP_SRV_SendData(
    dataLink_if_t *self,
    message_t *sentMsg,
    message_t *recvdMsg)
{
    obd_status_t status;
    const obd_transport_ops_t *transportOps = self->pTransportOps;
    const obd_timing_ops_t *timingOps = self->pTimingOps;
    void *transHandle = self->pTransportHandle;
    void *timingHandle = self->pTimingHandle;
    bool (*IsTimeoutExpired)(void *pHandle) = self->pTimingOps->is_timeout_expired;
    uint8_t aSentMsg[256] = {0};
    size_t sentMsgLen = 0;

    L2_KWP_PrepareMessage(sentMsg, aSentMsg, &sentMsgLen);

    // Tester sends request
    status = L2_KWP_SendMessage(
        timingHandle,
        timingOps,
        transHandle,
        transportOps,
        aSentMsg,
        sentMsgLen);
    OBD2_ASSERT_OK(status);

    // TODO: Handle cases where ecu sends multiple messages
    {
        status = L2_KWP_RecvMessage(
            timingHandle,
            timingOps,
            transHandle,
            transportOps,
            recvdMsg);
        OBD2_ASSERT_OK(status);

        // P2 Timeout from ECU response to ECU response
        while(IsTimeoutExpired(timingHandle));
        self->pTimingOps->stop_timeout(timingHandle);
    }

    // P3 delay min until tester can send next request
    self->pTimingOps->delay_ms(self->pTimingHandle, KWP_P3_TIME_MIN);

    return status;
}

/******************************************* ISO 14230-2 Services ********************************************/

/********************************************* ISO 14230-2 INIT **********************************************/
#if defined(SPT_5BUAD_INIT)
static obd_status_t L2_KWP_5BaudInit(dataLink_if_t *self)
{
    (void)(self);

    return OBD_STATUS_OK;
}
#endif /* SPT_5BAUD_INIT */

static obd_status_t L2_KWP_FastInit(dataLink_if_t *self)
{
    void *transportHandle = self->pTransportHandle;
    obd_status_t (*TR_SendPulse)(void *handle, bool pulse) =
        self->pTransportOps->send_pulse;
    void (*TR_SwitchBaud)(void *handle, uint8_t mode) =
        self->pTransportOps->change_baud;
    obd_status_t status;

    L2_KWP_IdleBasedOnConnStatus(self);

    TR_SwitchBaud(transportHandle, FAST_INIT_WAKEUP_START);

    // ISO 14230 Fast Init: LOW for 25ms (TiniL)
    TR_SendPulse(transportHandle, PULSE_LOW);
    self->pTimingOps->delay_ms(self->pTimingHandle, 25);

    // ISO 14230 Fast Init: HIGH for 25ms (TiniH)
    TR_SendPulse(transportHandle, PULSE_HIGH);
    self->pTimingOps->delay_ms(self->pTimingHandle, 24);

    TR_SwitchBaud(transportHandle, FAST_INIT_WAKEUP_END);

    // For keeping the size of the function small the L2_KWP_SRV_StartCommunication
    // will do more than just sending the service
    status = L2_KWP_SRV_StartCommunication(self);

    return status;
}

static obd_status_t L2_KWP_Init(dataLink_if_t *self)
{
    obd_status_t status;

    status = L2_KWP_FastInit(self);

#if defined(SPT_5BAUD_INIT)
    if (OBD_STATUS_OK != status)
    {
        status = L2_KWP_5BaudInit(self);

        if (OBD_NOT_SUPPORTED != status)
            OBD2_ASSERT_OK(status);
    }
#endif /* SPT_5BAUD_INIT */

    return status;
}

/********************************************* ISO 14230-2 INIT **********************************************/

/* ================================================ MODULE API ============================================= */
obd_status_t l2_kwp_connect(dataLink_if_t *self)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t *)(self->pProtocolCtx);
    obd_status_t status;

    status = self->pTimingOps->timer_init(self->pTimingHandle);
    OBD2_ASSERT_OK(status);

    status = self->pTransportOps->init(self->pTransportHandle);
    OBD2_ASSERT_OK(status);

    status = L2_KWP_Init(self);
    OBD2_ASSERT_OK(status);

    ctx->conStatus.bits.CONN_OK = 1;

    return OBD_STATUS_OK;
}

obd_status_t l2_kwp_send_request(dataLink_if_t *self, const obd_request_t *req, size_t len)
{
    l2_kwp_ctx_t ctx = *(l2_kwp_ctx_t *)(self->pProtocolCtx);
    obd_status_t status;
    const obd_transport_ops_t *transportOps = self->pTransportOps;
    const obd_timing_ops_t *timingOps = self->pTimingOps;
    void *transHandle = self->pTransportHandle;
    void *timingHandle = self->pTimingHandle;
    uint8_t aMessage[256] = {0};
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

    status = L2_KWP_SendMessage(
        timingHandle,
        timingOps,
        transHandle,
        transportOps,
        aMessage,
        msgLen);
    OBD2_ASSERT_OK(status);

    return OBD_STATUS_OK;
}

obd_status_t l2_kwp_recv_response(dataLink_if_t *self, obd_response_t *resp, size_t *len)
{
    obd_status_t status;
    const obd_transport_ops_t *transportOps = self->pTransportOps;
    const obd_timing_ops_t *timingOps = self->pTimingOps;
    void *transHandle = self->pTransportHandle;
    void *timingHandle = self->pTimingHandle;
    message_t recvdMsg = {0};

    status = L2_KWP_RecvMessage(
        timingHandle,
        timingOps,
        transHandle,
        transportOps,
        &recvdMsg);
    OBD2_ASSERT_OK(status);

    *resp = recvdMsg.data.resp;
    *len = recvdMsg.data.len;

    if (resp->negative.negResp != 0x7F)
    {
        // do something
    }

    return OBD_STATUS_OK;
}
