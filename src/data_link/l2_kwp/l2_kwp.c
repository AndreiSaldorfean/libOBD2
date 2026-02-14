/* ================================================ INCLUDES =============================================== */
#include "l2_kwp.h"
#include "data_link_if.h"
#include "l2_kwp_msg.h"
#include "l2_kwp_timing.h"
#include "srv_status.h"
#include "timing_if.h"
#include "transport_if.h"
#include "utils.h"
#include "l2_kwp_srv.h"

/* ================================================= MACROS ================================================ */
#define PULSE_HIGH (1U)
#define PULSE_LOW  (0U)

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data);

static obd_status_t L2_KWP_SendMessage(void* timingHandle,const obd_timing_ops_t *pTimingOps, void* handle, const obd_transport_ops_t *transportOps, message_t* msg);
static obd_status_t L2_KWP_RecvMessage(void* timingHandle,const obd_timing_ops_t *pTimingOps, void* handle, const obd_transport_ops_t *transportOps, message_t* recvdMsg);

static obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t* self);
static obd_status_t L2_KWP_SRV_StopCommunication(dataLink_if_t* self);
#if defined(SPT_CHANGE_TIMING_PARAM)
static obd_status_t L2_KWP_SRV_AccessTimingParameter(dataLink_if_t* self);
#endif /* SPT_CHANGE_TIMING_PARAM */
static obd_status_t L2_KWP_SRV_SendData(dataLink_if_t* self, message_t* sentMsg, message_t* recvdMsg);

#if defined(SPT_5BUAD_INIT)
static obd_status_t L2_KWP_5BaudInit(dataLink_if_t *self);
#endif /* SPT_5BAUD_INIT */
static obd_status_t L2_KWP_FastInit(dataLink_if_t *self);
static obd_status_t L2_KWP_Init(dataLink_if_t *self);
static void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data)
{
    uint8_t *hdr = (uint8_t*)&header;
    uint8_t *req = (uint8_t*)&data;
    uint8_t checksum = 0;
    uint8_t headerLen = (header.len == 0) ? 3 : 4;

    for (uint8_t idx = 0; idx < headerLen; idx++)
    {
        checksum+= hdr[idx];
    }

    for (uint8_t idx = 0; idx < data.len; idx++)
    {
        checksum+= req[idx];
    }

    return checksum % 256;
}

static obd_status_t L2_KWP_SendMessage(void* timingHandle,const obd_timing_ops_t *pTimingOps, void* handle, const obd_transport_ops_t *transportOps, message_t* msg)
{
    uint8_t* pMsg = (uint8_t*)msg;
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


    for (int idx = 0; idx < 256; idx++)
    {
        status = transportOps->send_byte(handle, pMsg[idx]);
        OBD2_ASSERT_OK(status);
    }

    pTimingOps->start_timeout(timingHandle, KWP_P2_TIME_MAX, NULL, NULL);

    return OBD_STATUS_OK;
}

static obd_status_t L2_KWP_RecvMessage(void* timingHandle,const obd_timing_ops_t *pTimingOps, void* handle, const obd_transport_ops_t *transportOps, message_t* recvdMsg)
{
    uint8_t* pMsg = (uint8_t*)recvdMsg;
    bool (*IsTimeoutExpired)(void *pHandle) = pTimingOps->is_timeout_expired;
    obd_status_t status;
    uint32_t (*GetTimeMs)(void *pHandle) = pTimingOps->get_time_ms;
    uint32_t p2TimeElapsed = 0;
    uint8_t len = 255;

    status = transportOps->recv_byte(handle, pMsg);
    OBD2_ASSERT_OK(status);

    OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
    p2TimeElapsed = GetTimeMs(timingHandle);
    if (p2TimeElapsed < KWP_P2_TIME_MIN)
    {
        return OBD_GENERIC_ERROR;
    }

    for (int idx = 1; idx <= len; idx++)
    {
        pTimingOps->start_timeout(timingHandle, KWP_P1_TIME_MAX, NULL, NULL);
        status = transportOps->recv_byte(handle, pMsg + idx);
        OBD2_ASSERT_EQUAL(false, IsTimeoutExpired(timingHandle));
        OBD2_ASSERT_OK(status);
        pTimingOps->stop_timeout(timingHandle);
    }

    pTimingOps->start_timeout(timingHandle, KWP_P3_TIME_MAX, NULL, NULL);

    return OBD_STATUS_OK;
}

static void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self)
{
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t*)self->pProtocolCtx;
    void* timingHandle = self->pTimingHandle;
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

/******************************************* ISO 14230-2 Services ********************************************/
static obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t* self)
{
    message_t recvdMsg = {0};
    obd_status_t status;
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t*)self->pProtocolCtx;

    // Actual StartCommunication
    {
        header_t hdr =
        {
            .fmt     = {0x81},
            .trgAddr = 0x33,
            .srcAddr = 0xF1,
            .len     = 0,
        };

        message_t sentMsg =
        {
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

static obd_status_t L2_KWP_SRV_SendData(dataLink_if_t* self, message_t* sentMsg, message_t* recvdMsg)
{
    obd_status_t status;
    const obd_transport_ops_t *transportOps = self->pTransportOps;
    const obd_timing_ops_t *timingOps = self->pTimingOps;
    void* transHandle = self->pTransportHandle;
    void* timingHandle = self->pTimingHandle;

    status = L2_KWP_SendMessage(timingHandle, timingOps, transHandle, transportOps, sentMsg);
    OBD2_ASSERT_OK(status);

    status = L2_KWP_RecvMessage(timingHandle, timingOps, transHandle, transportOps, recvdMsg);
    OBD2_ASSERT_OK(status);

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
    void* transportHandle = self->pTransportHandle;
    obd_status_t (*TR_SendPulse)(void *handle, bool pulse) = self->pTransportOps->send_pulse;
    void (*TR_SwitchBaud)(void *handle, uint8_t mode) = self->pTransportOps->change_baud;
    obd_status_t status;
    l2_kwp_ctx_t *ctx = self->pProtocolCtx;

    L2_KWP_IdleBasedOnConnStatus(self);

    TR_SwitchBaud(transportHandle, FAST_INIT_WAKEUP_START);

    TR_SendPulse(transportHandle, PULSE_HIGH);

    // delay 25ms
    self->pTimingOps->delay_ms(self->pTimingHandle, 25);

    TR_SendPulse(transportHandle, PULSE_LOW);

    // delay 25ms
    self->pTimingOps->delay_ms(self->pTimingHandle, 25);

    TR_SwitchBaud(transportHandle, FAST_INIT_WAKEUP_END);

    // For keeping the size of the function small the L2_KWP_SRV_StartCommunication
    // will do more than just sending the service
    status = L2_KWP_SRV_StartCommunication(self);

    return status;
}

static obd_status_t L2_KWP_Init(dataLink_if_t *self)
{
    obd_status_t status;

    self->pTimingOps->timer_init(self->pTimingHandle);

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
    l2_kwp_ctx_t *ctx = (l2_kwp_ctx_t*)(self->pProtocolCtx);
    obd_status_t status;

    status = self->pTransportOps->init(self->pTransportHandle);
    OBD2_ASSERT_OK(status);

    status = L2_KWP_Init(self);
    OBD2_ASSERT_OK(status);

    ctx->conStatus.bits.CONN_OK = 1;

    return OBD_STATUS_OK;
}

obd_status_t l2_kwp_send_request(dataLink_if_t *self, const obd_request_t *req, size_t len)
{
    l2_kwp_ctx_t ctx = *(l2_kwp_ctx_t*)(self->pProtocolCtx);
    obd_status_t status;
    const obd_transport_ops_t *transportOps = self->pTransportOps;
    const obd_timing_ops_t *timingOps = self->pTimingOps;
    void* transHandle = self->pTransportHandle;
    void* timingHandle = self->pTimingHandle;


    data_t data =
    {
        .req = *req,
        .len = len
    };

    // Construct the message
    message_t message =
    {
        .header = ctx.header,
        .data = data,
        .cs = L2_KWP_ComputeChecksum(ctx.header, data)
    };

    status = L2_KWP_SendMessage(timingHandle, timingOps, transHandle, transportOps, &message);
    OBD2_ASSERT_OK(status);

    return OBD_STATUS_OK;
}

obd_status_t l2_kwp_recv_response(dataLink_if_t *self, obd_response_t *resp, size_t* len)
{
    obd_status_t status;
    const obd_transport_ops_t *transportOps = self->pTransportOps;
    const obd_timing_ops_t *timingOps = self->pTimingOps;
    void* transHandle = self->pTransportHandle;
    void* timingHandle = self->pTimingHandle;
    message_t recvdMsg = {0};

    status = L2_KWP_RecvMessage(timingHandle, timingOps, transHandle, transportOps, &recvdMsg);
    OBD2_ASSERT_OK(status);

    *resp = recvdMsg.data.resp;
    *len = recvdMsg.data.len;

    if (resp->negative.negResp != 0x7F)
    {
        // do something
    }

    return OBD_STATUS_OK;
}
