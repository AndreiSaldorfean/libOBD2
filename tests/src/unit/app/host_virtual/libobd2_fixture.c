/* ================================================ INCLUDES =============================================== */
#include "libobd2_fixture.h"
#include "iso15031_5.h"
#include "uart_trace.h"
#include "unity.h"
#include <string.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
static const header_t header_ecu    = { .fmt = 0x48, .trgAddr = 0x6B, .srcAddr = 0x12 };
static const header_t header_tester = { .fmt = 0x68, .trgAddr = 0x6A, .srcAddr = 0xF1 };

/* ============================================ GLOBAL VARIABLES =========================================== */
appFixture_t appFx;

const uint8_t appFxVin[APPFX_VIN_MSG_CNT * 4u] = { 0x00, 0x00, 0x00, '1', 'H', 'G', 'C', 'M', '8', '2',
                                                   '6', '3', '3', 'A', '0', '0', '4', '3', '5', '2' };

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t ISO9141_ComputeChecksum(const header_t *header, const obd_data_t *data, size_t dataLen);

static obd_tmr_status_t keepTimer(void *pHandle);
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
    appFx.ctx = (obd_ctx_t){ .pDataLink = &appFx.link, .connectionStatus = 0 };
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
