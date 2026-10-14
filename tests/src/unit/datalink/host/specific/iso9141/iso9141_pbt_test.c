/* ================================================ INCLUDES =============================================== */
#include "iso9141_pbt_test.h"
#include "datalink.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "theft.h"
#include "timing_if.h"
#include "uart_if.h"
#include "uart_trace.h"
#include "unity.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

/* ================================================= MACROS ================================================ */
#define PBT_QUEUE_SIZE    4u
#define PBT_TRIALS        500u
#define MSG_DATA_LEN      7u  /* sid + 6 params, matches only-supported-protocol convention */
#define MSG_WIRE_LEN      11u /* 3 header + 7 data + 1 checksum */
#define MSG_QUEUE_SIZE    16u

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen);
extern void L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);
extern obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t *len);
static enum theft_alloc_res alloc_arr(struct theft *t, void *env, void **output);
static enum theft_alloc_res alloc_time(struct theft *t, void *env, void **output);
static void message_print(FILE *f, const void *p, void *env);
static bool hasPrematureChecksumMatch(header_t header, obd_data_t data, size_t dataLen);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info message_type_info = {
    .alloc = alloc_arr,
    .free  = theft_generic_free_cb,
    .print = message_print,
};

static struct theft_type_info time_type_info = {
    .alloc = alloc_time,
    .free  = theft_generic_free_cb,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void message_print(FILE *f, const void *p, void *env)
{
    (void)env;
    const message_t *m = (const message_t *)p;

    fprintf(f, "{\n");
    fprintf(f, "  header = { fmt=0x%02x, trg=0x%02x, src=0x%02x, len=%u },\n",
            m->header.fmt, m->header.trgAddr, m->header.srcAddr, m->header.len);
    fprintf(f, "  data = { sid=0x%02x, param=[0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x] },\n",
            m->data.sid,
            m->data.param[0], m->data.param[1], m->data.param[2],
            m->data.param[3], m->data.param[4], m->data.param[5],
            m->data.param[6]);
    fprintf(f, "  cs=0x%02x\n", m->cs);
    fprintf(f, "}");
}

static uint8_t TESTUTIL_ComputeChecksum(uint8_t arr[], size_t size)
{
    uint8_t checksum  = 0;

    for (uint8_t idx = 0; idx < size - 1; idx++)
    {
        checksum += arr[idx];
    }

    return checksum % 256;
}

/* --------------------------------------------------------------------------
 * ISO9141 has no length field: a receiver frames a message by reading bytes
 * until one equals the running checksum-so-far. If a data byte coincides
 * with the running sum before the real checksum is sent, the receiver would
 * frame the message early. Property tests that rely on the receiver parsing
 * the exact message that was sent must skip such (rare, ~1/256 per byte)
 * generated cases rather than report a false failure.
 * -------------------------------------------------------------------------- */
static bool hasPrematureChecksumMatch(header_t header, obd_data_t data, size_t dataLen)
{
    uint8_t *dataBytes = (uint8_t *)&data;
    uint8_t runningCheckSum = (uint8_t)(header.fmt + header.trgAddr + header.srcAddr);

    for (size_t idx = 0; idx < dataLen; idx++)
    {
        if (dataBytes[idx] == runningCheckSum) { return true; }
        runningCheckSum = (uint8_t)(runningCheckSum + dataBytes[idx]);
    }
    return false;
}

/* --------------------------------------------------------------------------
 * Allocator: generates a random obd message.
 * -------------------------------------------------------------------------- */
static enum theft_alloc_res alloc_arr(struct theft *t, void *env, void **output)
{
    (void)env;
    uint8_t *v = malloc(sizeof(message_t));
    if (v == NULL) { return THEFT_ALLOC_ERROR; }
    for (uint8_t idx = 0; idx < sizeof(message_t); idx++)
    {
        v[idx] = theft_random_bits(t, 8 * sizeof(uint8_t));
    }
    *output = v;
    return THEFT_ALLOC_OK;
}

/* --------------------------------------------------------------------------
 * Allocator: generates a random uint8_t for theft to use as the byte value.
 * -------------------------------------------------------------------------- */
static enum theft_alloc_res alloc_byte(struct theft *t, void *env, void **output)
{
    (void)env;
    uint8_t *v = malloc(sizeof(uint8_t));
    if (v == NULL) { return THEFT_ALLOC_ERROR; }
    *v = (uint8_t)theft_random_bits(t, 8);
    *output = v;
    return THEFT_ALLOC_OK;
}

/* --------------------------------------------------------------------------
 * Allocator: generates a random uint8_t for theft to use as the time value.
 * -------------------------------------------------------------------------- */
static enum theft_alloc_res alloc_time(struct theft *t, void *env, void **output)
{
    (void)env;
    uint8_t *v = malloc(sizeof(uint32_t));
    if (v == NULL) { return THEFT_ALLOC_ERROR; }
    *v = (uint32_t)theft_random_bits(t, 32);
    *output = v;
    return THEFT_ALLOC_OK;
}

/* --------------------------------------------------------------------------
 * Property 1: write any byte, advance time past its send time → read same byte
 * Steps:
 * 1. Write byte (T = 0)
 * 1. Advance time (T = 1)
 * 2. Read byte
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_message_checksum(struct theft *t, void *arg)
{
    (void)t;
    message_t msg      = *(message_t *)arg;
    uint8_t aSentMsg[11] = {0};
    size_t len        = 0;
    uint8_t realCs     = 0;
    uint8_t expectedCs = 0;

    L2_ISO9141_PrepareMessage(&msg, aSentMsg, 7, &len);

    /* aSentMsg holds 11 bytes: 3 header + sid + 6 params + cs. The checksum
     * covers everything but the trailing cs byte itself, i.e. the first 10
     * bytes -- TESTUTIL_ComputeChecksum sums (size - 1) bytes, so size=11. */
    expectedCs = TESTUTIL_ComputeChecksum(aSentMsg, 11);

    realCs     = L2_ISO9141_ComputeChecksum(msg.header, msg.data, 7);

    if (realCs != expectedCs) { return THEFT_TRIAL_FAIL; }
    return THEFT_TRIAL_PASS;
}

/* --------------------------------------------------------------------------
 * Property: L2_ISO9141_PrepareMessage must serialize a message_t into the
 * exact wire layout the protocol defines: [fmt, trg, src, sid, param x6, cs],
 * with the reported length always equal to MSG_WIRE_LEN.
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_prepare_message_serializes_correctly(struct theft *t, void *arg)
{
    (void)t;
    message_t msg = *(message_t *)arg;
    uint8_t aSentMsg[MSG_WIRE_LEN] = {0};
    size_t len = 0;

    L2_ISO9141_PrepareMessage(&msg, aSentMsg, MSG_DATA_LEN, &len);

    if (len != MSG_WIRE_LEN) { return THEFT_TRIAL_FAIL; }

    if (aSentMsg[0] != msg.header.fmt)     { return THEFT_TRIAL_FAIL; }
    if (aSentMsg[1] != msg.header.trgAddr) { return THEFT_TRIAL_FAIL; }
    if (aSentMsg[2] != msg.header.srcAddr) { return THEFT_TRIAL_FAIL; }
    if (aSentMsg[3] != msg.data.sid)       { return THEFT_TRIAL_FAIL; }

    for (uint8_t idx = 0; idx < MSG_DATA_LEN - 1u; idx++)
    {
        if (aSentMsg[4u + idx] != msg.data.param[idx]) { return THEFT_TRIAL_FAIL; }
    }

    if (aSentMsg[MSG_WIRE_LEN - 1u] != msg.cs) { return THEFT_TRIAL_FAIL; }

    return THEFT_TRIAL_PASS;
}

/* --------------------------------------------------------------------------
 * Property: a message serialized by L2_ISO9141_PrepareMessage and delivered
 * byte-by-byte over the virtual UART must come back out of
 * L2_ISO9141_RecvMessage unchanged: same header, same data, same length.
 * This exercises L2_ISO9141_ReadHeader's P1/P2 timing checks together with
 * the checksum-terminated data loop, end to end, without touching real time.
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_send_then_recv_roundtrip(struct theft *t, void *arg)
{
    (void)t;
    message_t msg = *(message_t *)arg;
    msg.cs = L2_ISO9141_ComputeChecksum(msg.header, msg.data, MSG_DATA_LEN);

    if (hasPrematureChecksumMatch(msg.header, msg.data, MSG_DATA_LEN)) { return THEFT_TRIAL_SKIP; }

    uint8_t aSentMsg[MSG_WIRE_LEN] = {0};
    size_t wireLen = 0;
    L2_ISO9141_PrepareMessage(&msg, aSentMsg, MSG_DATA_LEN, &wireLen);

    virtTmrCtx_t      timer                 = { .timeNow = 0u, .timeStep = 0u };
    virtUartLinkCtx_t uartCtx =
    {
        .tmrHandle      = &timer,
    };

    LIBOBD2_UART_Init(&uartCtx);

    for (size_t idx = 0; idx < wireLen; idx++)
    {
        LIBOBD2_UART_WriteByte(&uartCtx, aSentMsg[idx]);
    }

    obd_uart_ops_t uartOps = { .recv_byte = LIBOBD2_UART_RecvByte };
    obd_timing_ops_t timingOps =
    {
        .get_time_ms       = LIBOBD2_TMR_GetTimeMs,
        .delay_ms          = LIBOBD2_TMR_DelayMs,
        .start_timeout     = LIBOBD2_TMR_StartTimeout,
        .stop_timeout      = LIBOBD2_TMR_StopTimeout,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
    };
    dataLink_if_t linkCtx =
    {
        .pUartOps         = &uartOps,
        .pTransportHandle = &uartCtx,
        .pTimingOps       = &timingOps,
        .pTimingHandle    = &timer,
    };
    /* The LIBOBD_* macros expand to `handle->member` unparenthesized, so
     * `handle` must be a bare pointer expression -- not `&linkCtx` (`->`
     * binds tighter than unary `&`, so that would silently misparse). */
    dataLink_if_t *self = &linkCtx;

    /* Mirrors what a prior send (L2_ISO9141_SendMessage) leaves behind:
     * a time sample and a running P2 timeout for the upcoming response. */
    LIBOBD_SetTimeSample(self, LIBOBD_GetTimeMs(self));
    LIBOBD_StartTimeout(self, P2_TIME_MAX);
    LIBOBD_Delay(self, P2_TIME_MIN);

    message_t recvdMsg = {0};
    size_t recvdLen = 0;
    obd_status_t status = L2_ISO9141_RecvMessage(self, &recvdMsg, &recvdLen);

    if (status.response != OBD_STATUS_OK)           { return THEFT_TRIAL_FAIL; }
    if (status.timeout   != OBD_STATUS_OK)           { return THEFT_TRIAL_FAIL; }
    if (recvdLen != MSG_DATA_LEN)                    { return THEFT_TRIAL_FAIL; }
    if (recvdMsg.header.fmt != msg.header.fmt)       { return THEFT_TRIAL_FAIL; }
    if (recvdMsg.header.trgAddr != msg.header.trgAddr) { return THEFT_TRIAL_FAIL; }
    if (recvdMsg.header.srcAddr != msg.header.srcAddr) { return THEFT_TRIAL_FAIL; }
    if (recvdMsg.data.sid != msg.data.sid)           { return THEFT_TRIAL_FAIL; }
    if (recvdMsg.cs != msg.cs)                       { return THEFT_TRIAL_FAIL; }

    for (uint8_t idx = 0; idx < MSG_DATA_LEN - 1u; idx++)
    {
        if (recvdMsg.data.param[idx] != msg.data.param[idx]) { return THEFT_TRIAL_FAIL; }
    }

    return THEFT_TRIAL_PASS;
}

/* --------------------------------------------------------------------------
 * Property 2: when a byte is read time has to pass by one increment
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_inc_time_on_byte_read(struct theft *t, void *arg)
{
    (void)t;
    uint8_t byte_to_write = *(uint8_t *)arg;

    virtTmrCtx_t timer =
    {
        .timeNow  = 0u,
        .timeStep = 10u
    };
    virtUartLinkCtx_t ctx = {
        .tmrHandle      = &timer,
    };

    LIBOBD2_UART_Init(&ctx);

    LIBOBD2_UART_WriteByte(&ctx, byte_to_write);   /* recorded at timeNow=0 */
    /* do NOT advance the timer — timeNow still equals the event timestamp */

    uint8_t received = 0u;
    bool got_byte = LIBOBD2_UART_RecvByte(&ctx, &received);

    /* isByteReady uses strict '>': byte must NOT be ready at the same timestamp */
    if (timer.timeNow != timer.timeStep) { return THEFT_TRIAL_FAIL; }
    return THEFT_TRIAL_PASS;
}

/* --------------------------------------------------------------------------
 * Property 3: a byte written at T=0 must NOT be receivable when timeNow=0
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_byte_not_ready_at_same_timestamp(struct theft *t, void *arg)
{
    (void)t;
    uint8_t byte_to_write = *(uint8_t *)arg;

    uartEvent_t       queue[PBT_QUEUE_SIZE] = {0};
    virtTmrCtx_t      timer;
    virtUartLinkCtx_t ctx = {
        .tmrHandle = &timer, /* Sync uart events with correct timing */
    };

    LIBOBD2_TMR_Init(&timer);
    LIBOBD2_UART_Init(&ctx);

    LIBOBD2_UART_WriteByte(&ctx, byte_to_write);   /* recorded at timeNow=0 */
    /* do NOT advance the timer — timeNow still equals the event timestamp */

    uint8_t received = 0u;
    bool got_byte = LIBOBD2_UART_RecvByte(&ctx, &received);

    /* isByteReady uses strict '>': byte must NOT be ready at the same timestamp */
    if (got_byte) { return THEFT_TRIAL_FAIL; }
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
void test_ISO9141_PBT_ComputeCheckSum(void)
{
    struct theft_run_config cfg = {
        .name      = "Checksum func must equal checksum",
        .prop1     = prop_message_checksum,
        .type_info = { &message_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, theft_run(&cfg));
}

void test_ISO9141_PBT_PrepareMessageSerializesCorrectly(void)
{
    struct theft_run_config cfg = {
        .name      = "PrepareMessage serializes header/data/cs into wire layout",
        .prop1     = prop_prepare_message_serializes_correctly,
        .type_info = { &message_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, theft_run(&cfg));
}

void test_ISO9141_PBT_SendThenRecvRoundtrip(void)
{
    struct theft_run_config cfg = {
        .name      = "A prepared message read back over the virtual UART is unchanged",
        .prop1     = prop_send_then_recv_roundtrip,
        .type_info = { &message_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, runTraced(&cfg, "test_ISO9141_PBT_SendThenRecvRoundtrip_log.txt"));
}
