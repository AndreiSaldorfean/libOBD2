/* ================================================ INCLUDES =============================================== */
#include "iso9141_pbt_test.h"
#include "datalink.h"
#include "libobd2_uart_port.h"
#include "theft.h"
#include "unity.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

/* ================================================= MACROS ================================================ */
#define PBT_QUEUE_SIZE    4u
#define PBT_TRIALS        500u

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen);
extern void L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen, size_t *len);
static enum theft_alloc_res alloc_arr(struct theft *t, void *env, void **output);
static enum theft_alloc_res alloc_time(struct theft *t, void *env, void **output);
static void message_print(FILE *f, const void *p, void *env);

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

    expectedCs = TESTUTIL_ComputeChecksum(aSentMsg, 10);   /* recorded at timeNow=0 */

    realCs     = L2_ISO9141_ComputeChecksum(msg.header, msg.data, 7);   /* recorded at timeNow=0 */

    if (realCs != expectedCs) { return THEFT_TRIAL_FAIL; }
    return THEFT_TRIAL_PASS;
}

/* --------------------------------------------------------------------------
 * Property 2: when a byte is read time has to pass by one increment
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_inc_time_on_byte_read(struct theft *t, void *arg)
{
    (void)t;
    uint8_t byte_to_write = *(uint8_t *)arg;

    uartEvent_t queue[PBT_QUEUE_SIZE] = {0};
    virtTmrCtx_t timer =
    {
        .timeNow  = 0u,
        .timeStep = 10u
    };
    virtUartLinkCtx_t ctx = {
        .uartEventQueue = queue,
        .queueSize      = PBT_QUEUE_SIZE,
        .head           = 0u,
        .tail           = 0u,
        .tmrHandle      = &timer,
    };

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
    virtTmrCtx_t      timer                 = { .timeNow = 0u };
    virtUartLinkCtx_t ctx = {
        .uartEventQueue = queue,
        .queueSize      = PBT_QUEUE_SIZE,
        .head           = 0u,
        .tail           = 0u,
        .tmrHandle      = &timer,
    };

    LIBOBD2_UART_WriteByte(&ctx, byte_to_write);   /* recorded at timeNow=0 */
    /* do NOT advance the timer — timeNow still equals the event timestamp */

    uint8_t received = 0u;
    bool got_byte = LIBOBD2_UART_RecvByte(&ctx, &received);

    /* isByteReady uses strict '>': byte must NOT be ready at the same timestamp */
    if (got_byte) { return THEFT_TRIAL_FAIL; }
    return THEFT_TRIAL_PASS;
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

