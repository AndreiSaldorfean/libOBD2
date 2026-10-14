/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "dl_pbt_test.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "theft.h"
#include "timing_if.h"
#include "uart_if.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdint.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define PBT_QUEUE_SIZE 4u
#define PBT_TRIALS     500u

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
typedef struct
{
    uint8_t  byte;
    uint32_t timeStep;
    uint32_t timeMaxMargin;
} readByteCase_t;

typedef struct
{
    uint16_t timeMax;
    uint32_t timeStepMargin;
} timeoutCase_t;

static enum theft_alloc_res alloc_readByteCase(struct theft *t, void *env, void **output);
static enum theft_alloc_res alloc_timeoutCase(struct theft *t, void *env, void **output);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info readByteCase_type_info = {
    .alloc = alloc_readByteCase,
    .free  = theft_generic_free_cb,
};

static struct theft_type_info timeoutCase_type_info = {
    .alloc = alloc_timeoutCase,
    .free  = theft_generic_free_cb,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */

/* --------------------------------------------------------------------------
 * Allocator: a byte to deliver plus a virtual time step small enough that
 * the whole scenario stays well under MAX_BLOCKING_RECV_TIME.
 * -------------------------------------------------------------------------- */
static enum theft_alloc_res alloc_readByteCase(struct theft *t, void *env, void **output)
{
    (void)env;
    readByteCase_t *v = malloc(sizeof(readByteCase_t));
    if (v == NULL) { return THEFT_ALLOC_ERROR; }

    v->byte          = (uint8_t)theft_random_bits(t, 8);
    v->timeStep      = 1u + (uint32_t)(theft_random_bits(t, 8) % 50u);
    v->timeMaxMargin = 1u + (uint32_t)(theft_random_bits(t, 16) % 500u);

    *output = v;
    return THEFT_ALLOC_OK;
}

/* --------------------------------------------------------------------------
 * Allocator: a small timeMax paired with a strictly larger time step, so a
 * single failed poll is guaranteed to trip the timeout on the first check.
 * -------------------------------------------------------------------------- */
static enum theft_alloc_res alloc_timeoutCase(struct theft *t, void *env, void **output)
{
    (void)env;
    timeoutCase_t *v = malloc(sizeof(timeoutCase_t));
    if (v == NULL) { return THEFT_ALLOC_ERROR; }

    v->timeMax        = (uint16_t)(theft_random_bits(t, 8) % 100u);
    v->timeStepMargin = 1u + (uint32_t)(theft_random_bits(t, 8) % 100u);

    *output = v;
    return THEFT_ALLOC_OK;
}

/* --------------------------------------------------------------------------
 * Property 1: a byte pushed onto the link before the read starts is always
 * delivered back by ReadByteInTimeframe, with OBD_STATUS_OK, as long as the
 * timeout window is wider than a single virtual time step.
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_ReadByteInTimeframe_returns_byte_within_window(struct theft *t, void *arg)
{
    (void)t;
    readByteCase_t testCase = *(readByteCase_t *)arg;
    uint16_t timeMax = (uint16_t)(testCase.timeStep + testCase.timeMaxMargin);
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = testCase.timeStep };
    virtUartLinkCtx_t uartCtx =
    {
        .tmrHandle      = &timer,
    };
    obd_uart_ops_t uartOps = { .recv_byte = LIBOBD2_UART_RecvByte };
    obd_timing_ops_t timingOps =
    {
        .get_time_ms       = LIBOBD2_TMR_GetTimeMs,
        .delay_ms          = LIBOBD2_TMR_DelayMs,
        .start_timeout     = LIBOBD2_TMR_StartTimeout,
        .stop_timeout      = LIBOBD2_TMR_StopTimeout,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
    };
    dataLink_if_t self =
    {
        .pUartOps         = &uartOps,
        .pTransportHandle = &uartCtx,
        .pTimingOps       = &timingOps,
        .pTimingHandle    = &timer,
    };

    LIBOBD2_UART_Init(&uartCtx);
    LIBOBD2_UART_WriteByte(&uartCtx, testCase.byte);

    uint8_t received = (uint8_t)~testCase.byte;
    obd_status_t status = ReadByteInTimeframe(&self, NULL, &received, 0u, timeMax);

    if (status.response != OBD_STATUS_OK) { return THEFT_TRIAL_FAIL; }
    if (status.timeout   != OBD_STATUS_OK) { return THEFT_TRIAL_FAIL; }
    if (received != testCase.byte)         { return THEFT_TRIAL_FAIL; }
    return THEFT_TRIAL_PASS;
}

/* --------------------------------------------------------------------------
 * Property 2: when no byte is ever queued and the timeout window is
 * narrower than a single virtual time step, ReadByteInTimeframe must report
 * OBD_ERR_TIMEOUT_MAX and never fabricate a byte out of an empty queue slot.
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_ReadByteInTimeframe_times_out_without_byte(struct theft *t, void *arg)
{
    (void)t;
    timeoutCase_t testCase = *(timeoutCase_t *)arg;
    uint32_t timeStep = testCase.timeMax + testCase.timeStepMargin;
    virtTmrCtx_t timer =
    {
        .timeNow = 0u,
        .timeStep = timeStep
    };
    virtUartLinkCtx_t uartCtx =
    {
        .tmrHandle      = &timer,
    };

    LIBOBD2_UART_Init(&uartCtx);

    obd_uart_ops_t uartOps = { .recv_byte = LIBOBD2_UART_RecvByte };
    obd_timing_ops_t timingOps =
    {
        .get_time_ms       = LIBOBD2_TMR_GetTimeMs,
        .delay_ms          = LIBOBD2_TMR_DelayMs,
        .start_timeout     = LIBOBD2_TMR_StartTimeout,
        .stop_timeout      = LIBOBD2_TMR_StopTimeout,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
    };
    dataLink_if_t self =
    {
        .pUartOps         = &uartOps,
        .pTransportHandle = &uartCtx,
        .pTimingOps       = &timingOps,
        .pTimingHandle    = &timer,
    };

    /* Nothing is ever written to the link: byteReceived() must not treat
     * the empty, zero-initialized queue slot as an arrived byte. */
    uint8_t received = 0xAAu;
    obd_status_t status = ReadByteInTimeframe(&self, NULL, &received, 0u, testCase.timeMax);

    if (status.timeout != OBD_ERR_TIMEOUT_MAX) { return THEFT_TRIAL_FAIL; }
    return THEFT_TRIAL_PASS;
}

/* --------------------------------------------------------------------------
 * Property 3: RecvByteBlocking keeps polling until the queued byte becomes
 * visible, as long as it arrives well within MAX_BLOCKING_RECV_TIME.
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_RecvByteBlocking_returns_pushed_byte(struct theft *t, void *arg)
{
    (void)t;
    readByteCase_t testCase = *(readByteCase_t *)arg;

    uartEvent_t       queue[PBT_QUEUE_SIZE] = {0};
    virtTmrCtx_t      timer = { .timeNow = 0u, .timeStep = testCase.timeStep };
    virtUartLinkCtx_t uartCtx = {
        .tmrHandle = &timer, /* Sync uart events with correct timing */
    };

    LIBOBD2_UART_Init(&uartCtx);

    obd_uart_ops_t uartOps = { .recv_byte = LIBOBD2_UART_RecvByte };
    obd_timing_ops_t timingOps =
    {
        .get_time_ms       = LIBOBD2_TMR_GetTimeMs,
        .delay_ms          = LIBOBD2_TMR_DelayMs,
        .start_timeout     = LIBOBD2_TMR_StartTimeout,
        .stop_timeout      = LIBOBD2_TMR_StopTimeout,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
    };
    dataLink_if_t self =
    {
        .pUartOps         = &uartOps,
        .pTransportHandle = &uartCtx,
        .pTimingOps       = &timingOps,
        .pTimingHandle    = &timer,
    };

    LIBOBD2_UART_WriteByte(&uartCtx, testCase.byte);

    uint8_t received = (uint8_t)~testCase.byte;
    bool got = RecvByteBlocking(&self, &received);

    if (!got)                      { return THEFT_TRIAL_FAIL; }
    if (received != testCase.byte) { return THEFT_TRIAL_FAIL; }
    return THEFT_TRIAL_PASS;
}

/* --------------------------------------------------------------------------
 * Runs a theft config with the generic UART tracer (tests/src/ports/host/
 * uart_trace.*) enabled for its duration, logging every byte that crosses
 * LIBOBD2_UART_WriteByte()/LIBOBD2_UART_RecvByte() while it runs.
 * -------------------------------------------------------------------------- */
static enum theft_run_res runTraced(struct theft_run_config *cfg, const char *logPath)
{
    UART_TRACE_SetLogPath(logPath);
    g_uartTraceEnabled = true;

    enum theft_run_res res = theft_run(cfg);

    g_uartTraceEnabled = false;
    return res;
}

/* ================================================ MODULE API ============================================= */
void test_DL_PBT_ReadByteInTimeframe_ReturnsByteWithinWindow(void)
{
    struct theft_run_config cfg = {
        .name      = "ReadByteInTimeframe returns a queued byte within its window",
        .prop1     = prop_ReadByteInTimeframe_returns_byte_within_window,
        .type_info = { &readByteCase_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, runTraced(&cfg, "test_DL_PBT_ReadByteInTimeframe_ReturnsByteWithinWindow_log.txt"));
}

void test_DL_PBT_ReadByteInTimeframe_TimesOutWhenByteNeverArrives(void)
{
    struct theft_run_config cfg = {
        .name      = "ReadByteInTimeframe reports OBD_ERR_TIMEOUT_MAX with no byte queued",
        .prop1     = prop_ReadByteInTimeframe_times_out_without_byte,
        .type_info = { &timeoutCase_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, runTraced(&cfg, "test_DL_PBT_ReadByteInTimeframe_TimesOutWhenByteNeverArrives_log.txt"));
}

void test_DL_PBT_RecvByteBlocking_ReturnsPushedByte(void)
{
    struct theft_run_config cfg = {
        .name      = "RecvByteBlocking returns a byte queued before the call",
        .prop1     = prop_RecvByteBlocking_returns_pushed_byte,
        .type_info = { &readByteCase_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, runTraced(&cfg, "test_DL_PBT_RecvByteBlocking_ReturnsPushedByte_log.txt"));
}
