/* ================================================ INCLUDES =============================================== */
#include "libobd2_uart_port.h"
#include "libobd2_timer_port.h"
#include "uart_pbt_test.h"
#include "theft.h"
#include "unity.h"
#include <stdint.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define PBT_QUEUE_SIZE    4u
#define PBT_TRIALS        500u

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static enum theft_alloc_res alloc_byte(struct theft *t, void *env, void **output);
static enum theft_alloc_res alloc_time(struct theft *t, void *env, void **output);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info byte_type_info = {
    .alloc = alloc_byte,
    .free  = theft_generic_free_cb,
};

static struct theft_type_info time_type_info = {
    .alloc = alloc_time,
    .free  = theft_generic_free_cb,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */

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
static enum theft_trial_res prop_write_read_roundtrip(struct theft *t, void *arg)
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

    timer.timeNow = 1u;                            /* advance past event timestamp */

    uint8_t received = ~byte_to_write;             /* 2. pre-fill with wrong value */
    bool got_byte = LIBOBD2_UART_RecvByte(&ctx, &received);

    if (!got_byte)                  { return THEFT_TRIAL_FAIL; }
    if (received != byte_to_write)  { return THEFT_TRIAL_FAIL; }
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
void test_UART_PBT_WriteReadRoundtrip(void)
{
    struct theft_run_config cfg = {
        .name      = "Byte sent must be read after time passes",
        .prop1     = prop_write_read_roundtrip,
        .type_info = { &byte_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, theft_run(&cfg));
}

void test_UART_PBT_ByteNotReadyBeforeTimestamp(void)
{
    struct theft_run_config cfg = {
        .name      = "Byte not available at write time",
        .prop1     = prop_byte_not_ready_at_same_timestamp,
        .type_info = { &byte_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, theft_run(&cfg));
}

void test_UART_PBT_IncTimeOnByteRead(void)
{
    struct theft_run_config cfg = {
        .name      = "TMR time is incremented on RX poll",
        .prop1     = prop_inc_time_on_byte_read,
        .type_info = { &byte_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, theft_run(&cfg));
}
