/* ================================================ INCLUDES =============================================== */
#include "dl_fault_test.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "datalink_test_vectors_000.h"
#include "fault_injection.h"
#include "iso9141.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TIME_STEP     10u /* virtual ms consumed by every UART poll */
#define ECU_BYTE_GAP  5u  /* ECU inter-byte time (P1), below the poll step */
#define TARGET_ADDR   0x33u

/* ============================================ LOCAL VARIABLES ============================================ */
/* ECU answer to "01 05" (coolant temperature): 48 6B 12 41 05 7B 86 */
static const uint8_t coolantResp[] = { 0x48u, 0x6Bu, 0x12u, 0x41u, 0x05u, 0x7Bu, 0x86u };

static virtTmrCtx_t timer;
static virtUartLinkCtx_t uartCtx;
static iso9141_ctx_t protoCtx;
static dataLink_if_t link;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* Fresh virtual line with an ISO9141 tester link on it, using the real timer and UART ports */
static void setUpLine(void)
{
    timer    = (virtTmrCtx_t){ .timeStep = TIME_STEP };
    uartCtx  = (virtUartLinkCtx_t){ .tmrHandle = &timer };
    protoCtx = (iso9141_ctx_t){ .header = header_tester, .protocol = ISO9141 };
    link     = (dataLink_if_t)
    {
        .pProtocolCtx     = &protoCtx,
        .pTimingOps       = &tmr_ops,
        .pTimingHandle    = &timer,
        .pUartOps         = &uart_ops,
        .pTransportHandle = &uartCtx,
    };
    TEST_ASSERT_TRUE(UART_Init(&uartCtx));
}

/* Put an ECU frame on the line from just after P2 min, ECU_BYTE_GAP apart, then rewind the clock and
 * start P2 as a request that just ended would */
static void queueEcuFrame(const uint8_t *bytes, size_t len)
{
    for (size_t idx = 0; idx < len; idx++)
    {
        timer.timeNow = P2_TIME_MIN + TIME_STEP + (uint32_t)idx * ECU_BYTE_GAP;
        UART_WriteByte(&uartCtx, bytes[idx]);
    }
    timer.timeNow = 0u;
    (void)DL_StartTimeout(&link, P2_TIME_MAX);
}

/* Put the ECU side of a 5-baud init on the line: sync 55, keyword bytes 08 08, inverted address */
static void queueEcu5BaudInit(void)
{
    timer.timeNow = ISO9141_W5_TIME_MIN + 2000U + ISO9141_W1_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x55u);
    timer.timeNow += ISO9141_W2_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x08u);
    timer.timeNow += ISO9141_W3_TIME_MIN;
    UART_WriteByte(&uartCtx, 0x08u);
    timer.timeNow += 3u * ISO9141_W4_TIME_MIN;
    UART_WriteByte(&uartCtx, (uint8_t)(~TARGET_ADDR));
    timer.timeNow = 0u;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * A framing error on a data byte of the ECU response is reported as
 * OBD_ERR_UART_FRAMING, not as a broken frame or a timeout.
 *   ECU: 48 6B 12 41 [05 framing] 7B 86
 ******************************************************************/
void test_DLFAULT_RecvResponse_FramingError(void)
{
    faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_FRAMING_ERR, .firstByte = 4u, .count = 1u } };
    obd_data_t resp = {0};
    size_t len = 0u;

    UART_TRACE_Begin("test_DLFAULT_RecvResponse_FramingError.vcd");
    setUpLine();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&uartCtx, &inj));
    queueEcuFrame(coolantResp, sizeof(coolantResp));

    obd_status_t status = ISO9141_RecvResponse(&link, &resp, &len);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_FRAMING, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_UINT32(1u, inj.injected);
}

/******************************************************************
 * An overrun on the first byte of the response (the format byte) is
 * reported as OBD_ERR_UART_OVERRUN.
 ******************************************************************/
void test_DLFAULT_RecvResponse_OverrunOnFormatByte(void)
{
    faultUartInjector_t inj = { .onRead = { .error = OBD_UART_RX_OVERRUN_ERR, .count = 1u } };
    obd_data_t resp = {0};
    size_t len = 0u;

    UART_TRACE_Begin("test_DLFAULT_RecvResponse_OverrunOnFormatByte.vcd");
    setUpLine();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&uartCtx, &inj));
    queueEcuFrame(coolantResp, sizeof(coolantResp));

    obd_status_t status = ISO9141_RecvResponse(&link, &resp, &len);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_OVERRUN, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
}

/******************************************************************
 * A data byte corrupted without a UART error (05 -> 04) breaks the
 * checksum: no byte ends the frame, and the read times out waiting for
 * one more.
 ******************************************************************/
void test_DLFAULT_RecvResponse_CorruptedByte(void)
{
    faultUartInjector_t inj = { .onWrite = { .xorMask = 0x01u, .firstByte = 4u, .count = 1u } };
    obd_data_t resp = {0};
    size_t len = 0u;

    UART_TRACE_Begin("test_DLFAULT_RecvResponse_CorruptedByte.vcd");
    setUpLine();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&uartCtx, &inj));
    queueEcuFrame(coolantResp, sizeof(coolantResp));

    obd_status_t status = ISO9141_RecvResponse(&link, &resp, &len);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_DATA_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MAX, status.timeout);
}

/******************************************************************
 * A parity error on keyword byte 1 fails the 5-baud init with
 * OBD_ERR_UART_PARITY and stores no header.
 ******************************************************************/
void test_DLFAULT_Connect_ParityOnKeyByte(void)
{
    faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_PARITY_ERR, .firstByte = 1u, .count = 1u } };
    uint8_t protocol = 0xFFu;

    UART_TRACE_Begin("test_DLFAULT_Connect_ParityOnKeyByte.vcd");
    setUpLine();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&uartCtx, &inj));
    queueEcu5BaudInit();

    obd_status_t status = ISO9141_Connect(&link, &protocol);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_PARITY, status.response);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, protocol);
    TEST_ASSERT_EQUAL_HEX8(0x00u, protoCtx.header.fmt);
}

/******************************************************************
 * Noise on the sync byte (the first byte the tester reads) fails the
 * 5-baud init with OBD_ERR_UART_NOISE.
 ******************************************************************/
void test_DLFAULT_Connect_NoiseOnSyncByte(void)
{
    faultUartInjector_t inj = { .onRead = { .error = OBD_UART_RX_NOISE_ERR, .count = 1u } };
    uint8_t protocol = 0xFFu;

    UART_TRACE_Begin("test_DLFAULT_Connect_NoiseOnSyncByte.vcd");
    setUpLine();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&uartCtx, &inj));
    queueEcu5BaudInit();

    obd_status_t status = ISO9141_Connect(&link, &protocol);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_NOISE, status.response);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, protocol);
}

/******************************************************************
 * A timer that fails to init stops DL_Connect with
 * OBD_ERR_CONN_TIMER_INIT_FAILED before any protocol is tried.
 ******************************************************************/
void test_DLFAULT_Connect_TimerInitFails(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_INIT, .effect = FAULT_TMR_FAIL } };

    UART_TRACE_Begin("test_DLFAULT_Connect_TimerInitFails.vcd");
    setUpLine();
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));

    obd_status_t status = DL_Connect(&link);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_CONN_TIMER_INIT_FAILED, status.response);
    TEST_ASSERT_NULL(link.send_request);
    TEST_ASSERT_EQUAL_UINT32(0u, inj.calls[VIRT_TMR_OP_DELAY]);
}

/******************************************************************
 * The request goes out, but the P2 timeout for the answer cannot be
 * started: ISO9141_SendRequest reports OBD_ERR_TMR_START_FAILED.
 ******************************************************************/
void test_DLFAULT_SendRequest_TimerStartFails(void)
{
    const obd_data_t request = { .sid = 0x01u, .param = { 0x05u } };
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_START_TIMEOUT, .effect = FAULT_TMR_FAIL, .count = 1u } };

    UART_TRACE_Begin("test_DLFAULT_SendRequest_TimerStartFails.vcd");
    setUpLine();
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));

    obd_status_t status = ISO9141_SendRequest(&link, &request, 2u);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TMR_START_FAILED, status.response);
    TEST_ASSERT_EQUAL_UINT32(1u, inj.injected);
}

/******************************************************************
 * The format byte arrives, but the P2 timeout cannot be stopped:
 * ISO9141_RecvResponse reports OBD_ERR_TMR_STOP_FAILED.
 ******************************************************************/
void test_DLFAULT_RecvResponse_TimerStopFails(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_STOP_TIMEOUT, .effect = FAULT_TMR_FAIL } };
    obd_data_t resp = {0};
    size_t len = 0u;

    UART_TRACE_Begin("test_DLFAULT_RecvResponse_TimerStopFails.vcd");
    setUpLine();
    queueEcuFrame(coolantResp, sizeof(coolantResp));
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));

    obd_status_t status = ISO9141_RecvResponse(&link, &resp, &len);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TMR_STOP_FAILED, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
}

/******************************************************************
 * The P2 timeout expires before the ECU's format byte arrives: the
 * response is reported as missing with the P2 max timeout.
 ******************************************************************/
void test_DLFAULT_RecvResponse_TimeoutExpiresEarly(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_IS_TIMEOUT_EXPIRED, .effect = FAULT_TMR_EXPIRE } };
    obd_data_t resp = {0};
    size_t len = 0u;

    UART_TRACE_Begin("test_DLFAULT_RecvResponse_TimeoutExpiresEarly.vcd");
    setUpLine();
    queueEcuFrame(coolantResp, sizeof(coolantResp));
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));

    obd_status_t status = ISO9141_RecvResponse(&link, &resp, &len);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU, status.timeout);
}

/******************************************************************
 * The clock jumps back 40 ms when the P2 min check reads it, so the
 * ECU looks like it answered too early: the response is rejected with
 * the P2 min timeout.
 ******************************************************************/
void test_DLFAULT_RecvResponse_ClockJumpsBack(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_GET_TIME, .effect = FAULT_TMR_SHIFT, .shiftMs = -40, .count = 1u } };
    obd_data_t resp = {0};
    size_t len = 0u;

    UART_TRACE_Begin("test_DLFAULT_RecvResponse_ClockJumpsBack.vcd");
    setUpLine();
    queueEcuFrame(coolantResp, sizeof(coolantResp));
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));

    obd_status_t status = ISO9141_RecvResponse(&link, &resp, &len);
    UART_TRACE_End();

    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU, status.timeout);
}
