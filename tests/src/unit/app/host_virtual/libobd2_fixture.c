/* ================================================ INCLUDES =============================================== */
#include "libobd2_fixture.h"
#include "ecu_libobd2.h"
#include "iso15031_5.h"
#include "libobd2_priv.h"
#include "uart_trace.h"
#include "unity.h"
#include <string.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
static const header_t header_ecu    = { .fmt = 0x48, .trgAddr = 0x6B, .srcAddr = 0x12 };
static const header_t header_tester = { .fmt = 0x68, .trgAddr = 0x6A, .srcAddr = 0xF1 };

/* Placeholders behind the driver pointers of APPFX_MockLink(); never called */
static obd_timing_ops_t  unusedTmrOps;
static obd_uart_ops_t    unusedUartOps;
static virtTmrCtx_t      unusedTimer;
static virtUartLinkCtx_t unusedUart;

/* ============================================ GLOBAL VARIABLES =========================================== */
appFixture_t appFx;

const uint8_t appFxVin[APPFX_VIN_MSG_CNT * 4u] = { 0x00, 0x00, 0x00, '1', 'H', 'G', 'C', 'M', '8', '2',
                                                   '6', '3', '3', 'A', '0', '0', '4', '3', '5', '2' };

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* What LibOBD2_Init learnt about the ECU (libobd2.c) */
extern uint32_t supportedPids[MAX_NUM_OF_ECUS][MAX_NUM_OF_SIDS_WITH_PIDS][LIBOBD2_PID_RANGES];
extern bool supportedSids[MAX_NUM_OF_ECUS][MAX_NUM_OF_SIDS_WITH_PIDS];

extern uint8_t ISO9141_ComputeChecksum(const header_t *header, const obd_data_t *data, size_t dataLen);

static obd_tmr_status_t keepTimer(void *pHandle);
static obd_status_t scriptedSend(dataLink_if_t *pDataLink, const obd_data_t *req, size_t dataLen);
static obd_status_t scriptedRecv(dataLink_if_t *pDataLink, obd_data_t *resp, size_t *dataLen);
static bool keepLine(void *handle);
static void sendNoEcho(void *handle, uint8_t data);

static obd_timing_ops_t tmrOps =
{
    .timer_init         = keepTimer,
    .delay_ms           = TMR_DelayMs,
    .get_time_ms        = TMR_GetTimeMs,
    .is_timeout_expired = TMR_IsTimeoutExpired,
    .start_timeout      = TMR_StartTimeout,
    .stop_timeout       = TMR_StopTimeout,
};

static obd_uart_ops_t uartOps =
{
    .init        = keepLine,
    .send_byte   = UART_WriteByte,
    .recv_byte   = UART_RecvByte,
    .send_pulse  = UART_SendPulse,
    .switch_mode = UART_SwitchMode,
    .flush_rx    = UART_FlushRx,
};

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static obd_tmr_status_t keepTimer(void *pHandle)
{
    (void)pHandle;
    return OBD_TMR_OK;
}

static bool keepLine(void *handle)
{
    (void)handle;
    return true;
}

/* send_request of the scripted ECU: records the request */
static obd_status_t scriptedSend(dataLink_if_t *pDataLink, const obd_data_t *req, size_t dataLen)
{
    appFxScriptedEcu_t *ecu = (appFxScriptedEcu_t *)pDataLink->pProtocolCtx;
    obd_status_t status = {0};

    TEST_ASSERT_TRUE_MESSAGE(ecu->reqCnt < APPFX_ECU_MAX_REQS, "scripted ECU: too many requests");
    ecu->reqs[ecu->reqCnt]    = *req;
    ecu->reqLens[ecu->reqCnt] = dataLen;
    ecu->reqCnt++;

    status = ecu->sendStatus;
    return status;
}

/* recv_response of the scripted ECU: the scripted answer to the last request, else a P2 max timeout */
static obd_status_t scriptedRecv(dataLink_if_t *pDataLink, obd_data_t *resp, size_t *dataLen)
{
    const appFxScriptedEcu_t *ecu = (const appFxScriptedEcu_t *)pDataLink->pProtocolCtx;
    const ecu_libobd2_service_t *simAnswer = NULL;
    const obd_data_t *req = NULL;
    obd_status_t status = { .timeout = OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU,
                            .response = OBD_ERR_COMM_FMT_BYTE_NOT_RECVD };

    TEST_ASSERT_TRUE_MESSAGE(0u < ecu->reqCnt, "scripted ECU: response read before any request");
    req = &ecu->reqs[ecu->reqCnt - 1u];
    if (ecu->customSilence)
    {
        status = ecu->silentStatus;
    }

    for (size_t idx = 0u; idx < ecu->answerCnt; idx++)
    {
        if ((ecu->answers[idx].sid == req->sid) && (ecu->answers[idx].pid == req->param[0]))
        {
            *resp    = ecu->answers[idx].resp;
            *dataLen = ecu->answers[idx].respLen;
            status   = (obd_status_t){0};
            return status;
        }
    }

    simAnswer = ecu->useSimulator ? ECU_LibOBD2_GetAnswer(req->sid, req->param[0]) : NULL;
    if (NULL != simAnswer)
    {
        *resp    = simAnswer->serviceRequests.serviceRequests[0].data;
        *dataLen = simAnswer->serviceRequests.serviceRequests[0].dataLen;
        status   = (obd_status_t){0};
    }

    return status;
}

/* send_byte of a transceiver without echo: the byte goes out on the wire (trace) but is not received back */
static void sendNoEcho(void *handle, uint8_t data)
{
    const virtUartLinkCtx_t *uart = (const virtUartLinkCtx_t *)handle;

    UART_TRACE_OnWrite(data, uart->tmrHandle->timeNow, (uint8_t)OBD_UART_RX_OK);
}

/* ================================================ MODULE API ============================================= */
void APPFX_SetUpLine(void)
{
    uartOps.send_byte = UART_WriteByte;

    appFx.timer = (virtTmrCtx_t){ .timeStep = APPFX_TIME_STEP };
    appFx.uart.tmrHandle = &appFx.timer;
    appFx.uart.faults    = (virtUartFaultHooks_t){0};
    TEST_ASSERT_TRUE(UART_Init(&appFx.uart));

    appFx.proto = (iso9141_ctx_t){ .header = header_tester, .kb1 = 0x08u, .kb2 = 0x08u };
    appFx.link  = (dataLink_if_t)
    {
        .pProtocolCtx     = &appFx.proto,
        .pTimingOps       = &tmrOps,
        .pTimingHandle    = &appFx.timer,
        .pUartOps         = &uartOps,
        .pTransportHandle = &appFx.uart,
        .connect          = ISO9141_Connect,
        .send_request     = ISO9141_SendRequest,
        .recv_response    = ISO9141_RecvResponse,
    };
    appFx.ctx = (obd_ctx_t){ .pDataLink = &appFx.link, .connectionStatus = true };
    APPFX_SetEcuSupportsAll(true);
}

void APPFX_SetEcuSupportsAll(bool isSupported)
{
    for (size_t slot = 0u; slot < MAX_NUM_OF_SIDS_WITH_PIDS; slot++)
    {
        supportedSids[0][slot] = isSupported;
        for (size_t range = 0u; range < LIBOBD2_PID_RANGES; range++)
        {
            supportedPids[0][slot][range] = isSupported ? 0xFFFFFFFFu : 0u;
        }
    }
}

bool APPFX_HasEarlyChecksum(const uint8_t *data, size_t dataLen)
{
    uint8_t sum = (uint8_t)(header_ecu.fmt + header_ecu.trgAddr + header_ecu.srcAddr);

    for (size_t idx = 0u; idx < dataLen; idx++)
    {
        if (data[idx] == sum)
        {
            return true;
        }
        sum = (uint8_t)(sum + data[idx]);
    }
    return false;
}

obd_ctx_t APPFX_ConnectedCtx(dataLink_if_t *link)
{
    APPFX_SetEcuSupportsAll(true);
    return (obd_ctx_t){ .pDataLink = link, .connectionStatus = true };
}

dataLink_if_t APPFX_MockLink(void *ctx, dl_send_request_t send, dl_recv_response_t recv)
{
    return (dataLink_if_t)
    {
        .pProtocolCtx     = ctx,
        .pTimingOps       = &unusedTmrOps,
        .pTimingHandle    = &unusedTimer,
        .pUartOps         = &unusedUartOps,
        .pTransportHandle = &unusedUart,
        .send_request     = send,
        .recv_response    = recv,
    };
}

void APPFX_QueueIso9141Init(void)
{
    /* nothing is known about the protocol before the connect */
    appFx.proto = (iso9141_ctx_t){0};
    appFx.link.connect       = NULL;
    appFx.link.send_request  = NULL;
    appFx.link.recv_response = NULL;

    /* Same ECU timing as test_pos_ex_ISO9141_5BaudInit_0: W5 + 10 bits at 5 baud, then W1..W4 */
    appFx.timer.timeNow = ISO9141_W5_TIME_MIN + 2000u + ISO9141_W1_TIME_MIN;
    UART_WriteByte(&appFx.uart, 0x55u);
    appFx.timer.timeNow += ISO9141_W2_TIME_MIN;
    UART_WriteByte(&appFx.uart, 0x08u);
    appFx.timer.timeNow += ISO9141_W3_TIME_MIN;
    UART_WriteByte(&appFx.uart, 0x08u);
    appFx.timer.timeNow += 3u * ISO9141_W4_TIME_MIN;
    UART_WriteByte(&appFx.uart, (uint8_t)(~0x33u));
    appFx.timer.timeNow = 0u;
}

void APPFX_EcuReset(appFxScriptedEcu_t *ecu)
{
    memset(ecu, 0, sizeof(*ecu));
}

void APPFX_EcuAnswer(appFxScriptedEcu_t *ecu, uint8_t reqSid, uint8_t reqPid, const uint8_t *resp, size_t respLen)
{
    TEST_ASSERT_TRUE_MESSAGE(ecu->answerCnt < APPFX_ECU_MAX_ANSWERS, "scripted ECU: too many answers");
    TEST_ASSERT_TRUE_MESSAGE(respLen <= sizeof(obd_data_t), "scripted ECU: answer too long");

    ecu->answers[ecu->answerCnt].sid     = reqSid;
    ecu->answers[ecu->answerCnt].pid     = reqPid;
    ecu->answers[ecu->answerCnt].respLen = respLen;
    memcpy(&ecu->answers[ecu->answerCnt].resp, resp, respLen);
    ecu->answerCnt++;
}

dataLink_if_t APPFX_EcuLink(appFxScriptedEcu_t *ecu)
{
    return APPFX_MockLink(ecu, scriptedSend, scriptedRecv);
}

void APPFX_SuppressEcho(void)
{
    uartOps.send_byte = sendNoEcho;
}

uint32_t APPFX_QueueEcuFrame(uint32_t reqEnd, const uint8_t *data, size_t dataLen)
{
    uint8_t frame[APPFX_HDR_LEN + sizeof(obd_data_t) + 1u] = {0};
    obd_data_t obdData = {0};
    size_t frameLen = 0;
    uint32_t firstByteTime = reqEnd + P2_TIME_MIN;
    uint32_t firstPoll = 0u;

    memcpy(&obdData, data, dataLen);
    frame[frameLen++] = header_ecu.fmt;
    frame[frameLen++] = header_ecu.trgAddr;
    frame[frameLen++] = header_ecu.srcAddr;
    memcpy(&frame[frameLen], data, dataLen);
    frameLen += dataLen;
    frame[frameLen++] = ISO9141_ComputeChecksum(&header_ecu, &obdData, dataLen);

    for (size_t idx = 0; idx < frameLen; idx++)
    {
        appFx.timer.timeNow = firstByteTime + (uint32_t)idx * APPFX_ECU_BYTE_GAP;
        UART_WriteByte(&appFx.uart, frame[idx]);
    }
    appFx.timer.timeNow = 0u;

    /* first poll that sees the format byte, then one poll per byte */
    firstPoll = reqEnd + ((P2_TIME_MIN / APPFX_TIME_STEP) + 1u) * APPFX_TIME_STEP;
    return firstPoll + (uint32_t)frameLen * APPFX_TIME_STEP;
}

void APPFX_QueueVehicleInfo(uint8_t cntPid, uint8_t pid, const uint8_t *payload, uint8_t msgCnt, uint8_t sentCnt)
{
    const uint8_t sid = SID_REQUEST_VEHICLE_INFO + 0x40u;
    const uint8_t cntResp[] = { sid, cntPid, msgCnt };
    uint32_t time = 0u;

    /* tester asks for the message count */
    time += APPFX_REQ_DURATION(2u);
    time  = APPFX_QueueEcuFrame(time, cntResp, sizeof(cntResp));

    /* tester asks for the data, the ECU answers with every message back to back */
    time += APPFX_REQ_DURATION(2u);
    for (uint8_t msgNr = 1u; msgNr <= sentCnt; msgNr++)
    {
        const uint8_t *d = &payload[(msgNr - 1u) * 4u];
        const uint8_t msg[] = { sid, pid, msgNr, d[0], d[1], d[2], d[3] };
        time = APPFX_QueueEcuFrame(time, msg, sizeof(msg));
    }
}

uint32_t APPFX_QueueEcuExchange(const libobd2_service_t *service)
{
    const ecu_libobd2_service_t *answer = NULL;
    const libobd2_data_t *request = NULL;
    uint32_t time = 0u;

    for (size_t reqIdx = 0u; (reqIdx < LIBOBD2_MAX_SREQ) && (0u < service->serviceRequests[reqIdx].dataLen); reqIdx++)
    {
        request = &service->serviceRequests[reqIdx];
        answer  = ECU_LibOBD2_GetAnswer(request->data.sid, request->data.param[0]);

        /* tester sends the request, the ECU answers with every message back to back */
        time += APPFX_REQ_DURATION((uint32_t)request->dataLen);
        for (size_t msgIdx = 0u; (NULL != answer) && (msgIdx < answer->sreqLen); msgIdx++)
        {
            const libobd2_data_t *msg = &answer->serviceRequests.serviceRequests[msgIdx];
            time = APPFX_QueueEcuFrame(time, (const uint8_t *)&msg->data, msg->dataLen);
        }
    }

    return time;
}

bool APPFX_EcuExchangeHitsF1(const libobd2_service_t *service)
{
    const ecu_libobd2_service_t *answer = NULL;
    const libobd2_data_t *request = NULL;
    bool hits = false;

    for (size_t reqIdx = 0u; (reqIdx < LIBOBD2_MAX_SREQ) && (0u < service->serviceRequests[reqIdx].dataLen); reqIdx++)
    {
        request = &service->serviceRequests[reqIdx];
        answer  = ECU_LibOBD2_GetAnswer(request->data.sid, request->data.param[0]);
        for (size_t msgIdx = 0u; (NULL != answer) && (msgIdx < answer->sreqLen); msgIdx++)
        {
            const libobd2_data_t *msg = &answer->serviceRequests.serviceRequests[msgIdx];
            hits = hits || APPFX_HasEarlyChecksum((const uint8_t *)&msg->data, msg->dataLen);
        }
    }

    return hits;
}
