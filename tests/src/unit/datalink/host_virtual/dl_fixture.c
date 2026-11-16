/* ================================================ INCLUDES =============================================== */
#include "dl_fixture.h"
#include "datalink_test_vectors_000.h"
#include "datalink_utils.h"
#include "unity.h"

/* ================================================= MACROS ================================================ */
#define ECU_INIT_BYTES 4u

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
dlFixture_t dlFx;

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void recordPulse(void *handle, bool pulse);
static void recordMode(void *handle, uint8_t mode);

obd_timing_ops_t dlFxTmrOps =
{
    .timer_init         = DLFX_KeepTimer,
    .delay_ms           = TMR_DelayMs,
    .get_time_ms        = TMR_GetTimeMs,
    .is_timeout_expired = TMR_IsTimeoutExpired,
    .start_timeout      = TMR_StartTimeout,
    .stop_timeout       = TMR_StopTimeout,
};

/* The host port with the bit-bang calls recorded on the way through */
obd_uart_ops_t dlFxUartOps =
{
    .init        = DLFX_KeepLine,
    .send_byte   = UART_WriteByte,
    .recv_byte   = UART_RecvByte,
    .send_pulse  = recordPulse,
    .switch_mode = recordMode,
    .flush_rx    = UART_FlushRx,
};

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void recordPulse(void *handle, bool pulse)
{
    if (dlFx.pulseCnt < (DLFX_BITBANG_BITS + 1u))
    {
        dlFx.pulseLevel[dlFx.pulseCnt] = pulse;
        dlFx.pulseTime[dlFx.pulseCnt]  = dlFx.timer.timeNow;
        dlFx.pulseCnt++;
    }
    UART_SendPulse(handle, pulse);
}

static void recordMode(void *handle, uint8_t mode)
{
    if (dlFx.modeCnt < DLFX_MODE_SEQ_LEN)
    {
        dlFx.modeSeq[dlFx.modeCnt++] = mode;
    }
    UART_SwitchMode(handle, mode);
}

/* ================================================ MODULE API ============================================= */
void DLFX_SetUpConnectedLine(void)
{
    dlFx.timer = (virtTmrCtx_t){ .timeStep = DLFX_TIME_STEP };
    dlFx.uart.tmrHandle = &dlFx.timer;
    dlFx.uart.faults    = (virtUartFaultHooks_t){0};
    TEST_ASSERT_TRUE(UART_Init(&dlFx.uart));

    dlFx.proto    = (iso9141_ctx_t){ .header = header_tester, .kb1 = 0x08u, .kb2 = 0x08u };
    dlFx.pulseCnt = 0u;
    dlFx.modeCnt  = 0u;

    dlFx.link = (dataLink_if_t)
    {
        .pProtocolCtx     = &dlFx.proto,
        .pTimingOps       = &dlFxTmrOps,
        .pTimingHandle    = &dlFx.timer,
        .pUartOps         = &dlFxUartOps,
        .pTransportHandle = &dlFx.uart,
        .connect          = ISO9141_Connect,
        .send_request     = ISO9141_SendRequest,
        .recv_response    = ISO9141_RecvResponse,
    };
}

void DLFX_SetUpLine(void)
{
    dlFx.timer = (virtTmrCtx_t){ .timeStep = DLFX_TIME_STEP };
    dlFx.uart  = (virtUartLinkCtx_t){ .tmrHandle = &dlFx.timer };
    dlFx.proto = (iso9141_ctx_t){ .header = header_tester, .protocol = ISO9141 };
    dlFx.link  = (dataLink_if_t)
    {
        .pProtocolCtx     = &dlFx.proto,
        .pTimingOps       = &tmr_ops,
        .pTimingHandle    = &dlFx.timer,
        .pUartOps         = &uart_ops,
        .pTransportHandle = &dlFx.uart,
    };
    dlFx.pulseCnt = 0u;
    dlFx.modeCnt  = 0u;
    TEST_ASSERT_TRUE(UART_Init(&dlFx.uart));
}

void DLFX_LineWriteAt(uint32_t time, uint8_t byte)
{
    uint32_t now = dlFx.timer.timeNow;

    dlFx.timer.timeNow = time;
    UART_WriteByte(&dlFx.uart, byte);
    dlFx.timer.timeNow = now;
}

void DLFX_QueueEcuFrame(const uint8_t *bytes, size_t len)
{
    for (size_t idx = 0; idx < len; idx++)
    {
        DLFX_LineWriteAt(P2_TIME_MIN + DLFX_TIME_STEP + (uint32_t)idx * DLFX_ECU_BYTE_GAP, bytes[idx]);
    }
    dlFx.timer.timeNow = 0u;
    (void)DL_StartTimeout(&dlFx.link, P2_TIME_MAX);
}

void DLFX_QueueEcu5BaudInit(const dlFxEcuInit_t *ecu)
{
    const uint8_t bytes[ECU_INIT_BYTES] = { ecu->sync, ecu->kb1, ecu->kb2, ecu->invAddr };
    const uint32_t gaps[ECU_INIT_BYTES] =
    {
        ISO9141_W5_TIME_MIN + DLFX_BITBANG_BITS * DLFX_BITBANG_BIT_TIME + ISO9141_W1_TIME_MIN,
        ISO9141_W2_TIME_MIN,
        ISO9141_W3_TIME_MIN,
        3u * ISO9141_W4_TIME_MIN, /* tester waits W4, sends ~KB2, then the ECU waits W4 */
    };
    uint32_t time = 0u;

    for (uint8_t idx = 0u; idx < ECU_INIT_BYTES; idx++)
    {
        time += gaps[idx];
        if (idx == ecu->lateIdx)
        {
            time += ecu->lateMs;
        }
        DLFX_LineWriteAt(time, bytes[idx]);
    }
    dlFx.timer.timeNow = 0u;
}

obd_tmr_status_t DLFX_KeepTimer(void *pHandle)
{
    (void)pHandle;
    return OBD_TMR_OK;
}

bool DLFX_KeepLine(void *handle)
{
    (void)handle;
    return true;
}

void DLFX_FailStartTimeout(void *pUserData, virtTmrEvent_t *event)
{
    (void)pUserData;
    if (VIRT_TMR_OP_START_TIMEOUT == event->op)
    {
        event->status = OBD_TMR_ERR_HW;
    }
}

void DLFX_InjectFramingError(void *pUserData, uartEvent_t *event)
{
    (void)pUserData;
    event->error = OBD_UART_RX_FRAMING_ERR;
}
