/* ================================================ INCLUDES =============================================== */
#include "dl_test.h"
#include "datalink.h"
#include "l2_iso9141.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TIME_STEP          10u /* virtual ms consumed by every UART poll */
#define BITBANG_BITS       10u /* start + 8 data + stop */
#define BITBANG_BIT_TIME   (1000u / 5u)
#define TARGET_ADDR        0x33u

/* ============================================ LOCAL VARIABLES ============================================ */
static virtTmrCtx_t timer;
static virtUartLinkCtx_t uartCtx;
static l2_iso9141_ctx_t protoCtx;
static dataLink_if_t dataLink;

/* What the bit-banged byte looked like on the line */
static uint8_t  pulseLevel[BITBANG_BITS + 1u];
static uint32_t pulseTime[BITBANG_BITS + 1u];
static size_t   pulseCnt;
static uint8_t  modeSeq[4];
static size_t   modeCnt;

static const header_t header_tester =
{
    .fmt     = 0x68,
    .trgAddr = 0x6A,
    .srcAddr = 0xF1,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static bool keepTimer(void *pHandle);
static bool keepLine(void *handle);
static void recordPulse(void *handle, bool pulse);
static void recordMode(void *handle, uint8_t mode);

/* DL_Connect() re-initialises the timer and the transport. On the virtual line that would zero timeStep and
 * drop the ECU bytes the test queued, so the test owns both and these inits keep them as they are. */
static obd_timing_ops_t tmrOps =
{
    .timer_init         = keepTimer,
    .delay_ms           = LIBOBD2_TMR_DelayMs,
    .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
    .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
    .start_timeout      = LIBOBD2_TMR_StartTimeout,
    .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

/* The host port with the bit-bang calls recorded on the way through */
static obd_uart_ops_t uartOps =
{
    .init        = keepLine,
    .send_byte   = LIBOBD2_UART_WriteByte,
    .recv_byte   = LIBOBD2_UART_RecvByte,
    .send_pulse  = recordPulse,
    .switch_mode = recordMode,
    .flush_rx    = LIBOBD2_UART_FlushRx,
};

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static bool keepTimer(void *pHandle)
{
    (void)pHandle;
    return true;
}

static bool keepLine(void *handle)
{
    (void)handle;
    return true;
}

static void recordPulse(void *handle, bool pulse)
{
    if (pulseCnt < (BITBANG_BITS + 1u))
    {
        pulseLevel[pulseCnt] = pulse;
        pulseTime[pulseCnt]  = timer.timeNow;
        pulseCnt++;
    }
    LIBOBD2_UART_SendPulse(handle, pulse);
}

static void recordMode(void *handle, uint8_t mode)
{
    if (modeCnt < sizeof(modeSeq))
    {
        modeSeq[modeCnt++] = mode;
    }
    LIBOBD2_UART_SwitchMode(handle, mode);
}

/******************************************************************
 * Fresh virtual K-line: clock at 0, empty line, datalink bound to
 * ISO9141 with the 68 6A F1 header.
 ******************************************************************/
static void setUpLine(void)
{
    timer = (virtTmrCtx_t){ .timeStep = TIME_STEP };
    uartCtx.tmrHandle       = &timer;
    uartCtx.erorr_generator = NULL;
    TEST_ASSERT_TRUE(LIBOBD2_UART_Init(&uartCtx));

    protoCtx = (l2_iso9141_ctx_t){ .header = header_tester, .kb1 = 0x08u, .kb2 = 0x08u };
    tmrOps.timeSample = 0u;
    pulseCnt = 0u;
    modeCnt  = 0u;

    dataLink = (dataLink_if_t)
    {
        .pProtocolCtx     = &protoCtx,
        .pTimingOps       = &tmrOps,
        .pTimingHandle    = &timer,
        .pUartOps         = &uartOps,
        .pTransportHandle = &uartCtx,
        .connect          = l2_iso9141_connect,
        .send_request     = l2_iso9141_send_request,
        .recv_response    = l2_iso9141_recv_response,
    };
}

/* Put one byte on the line at virtual time `time`, without moving the clock */
static void lineWriteAt(uint32_t time, uint8_t byte)
{
    uint32_t now = timer.timeNow;

    timer.timeNow = time;
    LIBOBD2_UART_WriteByte(&uartCtx, byte);
    timer.timeNow = now;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * A byte that lands inside [timeMin, timeMax] is returned OK.
 *   1. Byte is on the line at t=30, window is [P2 min, P2 max]
 *   2. Tester polls at 0, 10, 20, 30 (not yet there) and 40 (read)
 *   3. Elapsed time is 50 ms: 5 polls of 10 ms
 ******************************************************************/
void test_DL_ReadByteInTimeframe_000(void)
{
    uint8_t byte = 0u;
    uint32_t elapsed = 0u;

    setUpLine();
    lineWriteAt(30u, 0x55u);

    obd_status_t status = ReadByteInTimeframe(&dataLink, &elapsed, &byte, P2_TIME_MIN, P2_TIME_MAX);

    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_HEX8(0x55u, byte);
    TEST_ASSERT_EQUAL_UINT32(5u * TIME_STEP, elapsed);
}

/******************************************************************
 * A byte that comes before timeMin is still read, but the call
 * reports a min timeout (ECU answered too early).
 ******************************************************************/
void test_DL_ReadByteInTimeframe_TooEarly(void)
{
    uint8_t byte = 0u;
    uint32_t elapsed = 0u;

    setUpLine();
    lineWriteAt(0u, 0x55u);

    obd_status_t status = ReadByteInTimeframe(&dataLink, &elapsed, &byte, P2_TIME_MIN, P2_TIME_MAX);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MIN, status.timeout);
    TEST_ASSERT_EQUAL_HEX8(0x55u, byte);
    TEST_ASSERT_LESS_THAN_UINT32(P2_TIME_MIN, elapsed);
}

/******************************************************************
 * Nothing on the line: the call gives up with a max timeout once
 * timeMax has passed, it does not block forever.
 ******************************************************************/
void test_DL_ReadByteInTimeframe_NoByte(void)
{
    const uint32_t start = 100u;
    const uint16_t timeMax = ISO9141_W4_TIME_MAX;
    uint8_t byte = 0u;

    setUpLine();
    timer.timeNow = start;

    obd_status_t status = ReadByteInTimeframe(&dataLink, NULL, &byte, ISO9141_W4_TIME_MIN, timeMax);

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MAX, status.timeout);
    TEST_ASSERT_GREATER_THAN_UINT32(start + timeMax, timer.timeNow);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(start + timeMax + TIME_STEP, timer.timeNow);
}

/******************************************************************
 * RecvByteBlocking waits as long as needed for the next byte.
 ******************************************************************/
void test_DL_RecvByteBlocking_000(void)
{
    uint8_t byte = 0u;

    setUpLine();
    lineWriteAt(1000u, 0xA5u);

    TEST_ASSERT_TRUE(RecvByteBlocking(&dataLink, &byte));
    TEST_ASSERT_EQUAL_HEX8(0xA5u, byte);
    TEST_ASSERT_GREATER_THAN_UINT32(1000u, timer.timeNow);
}

/******************************************************************
 * RecvByteBlocking still gives up after MAX_BLOCKING_RECV_TIME.
 ******************************************************************/
void test_DL_RecvByteBlocking_NoByte(void)
{
    uint8_t byte = 0u;

    setUpLine();

    TEST_ASSERT_FALSE(RecvByteBlocking(&dataLink, &byte));
    TEST_ASSERT_GREATER_THAN_UINT32(MAX_BLOCKING_RECV_TIME, timer.timeNow);
}

/******************************************************************
 * Send 0x33 at 5 baud, the ISO9141 wake-up address.
 *   1. Line switches to digital mode, then back at the end
 *   2. Start bit low, 8 data bits LSB first, stop bit high
 *   3. Every bit lasts 200 ms, 2 s for the whole byte
 ******************************************************************/
void test_DL_SendByteBitBanged_000(void)
{
    /* 0x33 = 0b00110011, LSB first */
    const uint8_t expLevel[BITBANG_BITS] = { PULSE_LOW,
                                             1, 1, 0, 0, 1, 1, 0, 0,
                                             PULSE_HIGH };

    setUpLine();

    UART_TRACE_Begin("test_DL_SendByteBitBanged_000.vcd");
    SendByteBitBanged(&dataLink, TARGET_ADDR, 5u);
    UART_TRACE_End();

    /* 1. */
    TEST_ASSERT_EQUAL_size_t(2u, modeCnt);
    TEST_ASSERT_EQUAL_UINT8(DIGITAL_MODE_BEGIN, modeSeq[0]);
    TEST_ASSERT_EQUAL_UINT8(DIGITAL_MODE_END, modeSeq[1]);

    /* 2. */
    TEST_ASSERT_EQUAL_size_t(BITBANG_BITS, pulseCnt);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expLevel, pulseLevel, BITBANG_BITS);

    /* 3. */
    for (size_t bit = 0; bit < BITBANG_BITS; bit++)
    {
        TEST_ASSERT_EQUAL_UINT32(bit * BITBANG_BIT_TIME, pulseTime[bit]);
    }
    TEST_ASSERT_EQUAL_UINT32(BITBANG_BITS * BITBANG_BIT_TIME, timer.timeNow);
}

/******************************************************************
 * Connect to an ISO9141-2 ECU through the common datalink.
 *   1. ECU answers the 5 baud init with 55 08 08 and CC
 *   2. DL_Connect must report OK and bind send/recv to ISO9141
 ******************************************************************/
void test_DL_Connect_000(void)
{
    setUpLine();
    dataLink.connect       = NULL;
    dataLink.send_request  = NULL;
    dataLink.recv_response = NULL;

    /* Same ECU timing as test_ISO9141_5BaudInit_000: W5 + 10 bits at 5 baud, then W1..W4 */
    uint32_t time = ISO9141_W5_TIME_MIN + BITBANG_BITS * BITBANG_BIT_TIME + ISO9141_W1_TIME_MIN;
    lineWriteAt(time, 0x55u);
    time += ISO9141_W2_TIME_MIN;
    lineWriteAt(time, 0x08u);
    time += ISO9141_W3_TIME_MIN;
    lineWriteAt(time, 0x08u);
    time += 3u * ISO9141_W4_TIME_MIN;
    lineWriteAt(time, (uint8_t)(~TARGET_ADDR));

    UART_TRACE_Begin("test_DL_Connect_000.vcd");
    obd_status_t status = DL_Connect(&dataLink);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_PTR(l2_iso9141_connect, dataLink.connect);
    TEST_ASSERT_EQUAL_PTR(l2_iso9141_send_request, dataLink.send_request);
    TEST_ASSERT_EQUAL_PTR(l2_iso9141_recv_response, dataLink.recv_response);
    TEST_ASSERT_EQUAL_HEX8(header_tester.fmt, protoCtx.header.fmt);
    TEST_ASSERT_EQUAL_HEX8(header_tester.trgAddr, protoCtx.header.trgAddr);
    TEST_ASSERT_EQUAL_HEX8(header_tester.srcAddr, protoCtx.header.srcAddr);
}

/******************************************************************
 * Send a request and read it back through the common datalink API.
 *   1. DL_SendRequest puts 68 6A F1 01 05 C9 on the line
 *   2. The host UART is a loopback, so after P2 min the frame is
 *      read back with DL_RecvResponse
 *   3. Only the data part (sid + params) is handed back
 ******************************************************************/
void test_DL_SendRecv_000(void)
{
    const obd_data_t request = { .sid = 0x01u, .param = { 0x05u } };
    obd_data_t resp = {0};
    size_t respLen = 0u;

    setUpLine();

    UART_TRACE_Begin("test_DL_SendRecv_000.vcd");
    obd_status_t status = DL_SendRequest(&dataLink, &request, 2u);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    LIBOBD_Delay((&dataLink), P2_TIME_MIN);

    status = DL_RecvResponse(&dataLink, &resp, &respLen);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(2u, respLen);
    TEST_ASSERT_EQUAL_HEX8(request.sid, resp.sid);
    TEST_ASSERT_EQUAL_HEX8(request.param[0], resp.param[0]);
}
