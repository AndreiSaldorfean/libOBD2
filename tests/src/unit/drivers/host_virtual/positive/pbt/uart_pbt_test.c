/* ================================================ INCLUDES =============================================== */
#include "uart_pbt_test.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "pbt_support.h"
#include "theft.h"
#include "unity.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define TIME_STEP     10u
#define MAX_BURST     32u  /* bytes in one generated burst, well below VIRT_UART_QUEUE_SIZE */
#define MAX_BYTE_GAP  50u  /* ms between two written bytes */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* A burst of bytes put on the line 'gap[i]' ms after the previous one */
typedef struct
{
    uint8_t  count;
    uint8_t  byte[MAX_BURST];
    uint32_t gap[MAX_BURST];
} burst_t;

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static enum theft_alloc_res allocByte(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocBurst(struct theft *t, void *env, void **output);
static void printByte(FILE *f, const void *instance, void *env);
static void printBurst(FILE *f, const void *instance, void *env);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info byteInfo  = PBT_TYPE_INFO(allocByte, printByte);
static struct theft_type_info burstInfo = PBT_TYPE_INFO(allocBurst, printBurst);

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static enum theft_alloc_res allocByte(struct theft *t, void *env, void **output)
{
    uint8_t *v = malloc(sizeof(*v));

    (void)env;
    if (v == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    *v = PBT_Byte(t);
    *output = v;
    return THEFT_ALLOC_OK;
}

static enum theft_alloc_res allocBurst(struct theft *t, void *env, void **output)
{
    burst_t *b = calloc(1u, sizeof(*b));

    (void)env;
    if (b == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    b->count = (uint8_t)PBT_Range(t, 1u, MAX_BURST);
    for (uint8_t idx = 0u; idx < b->count; idx++)
    {
        b->byte[idx] = PBT_Byte(t);
        b->gap[idx]  = PBT_Range(t, 0u, MAX_BYTE_GAP);
    }
    *output = b;
    return THEFT_ALLOC_OK;
}

static void printByte(FILE *f, const void *instance, void *env)
{
    (void)env;
    fprintf(f, "0x%02X", *(const uint8_t *)instance);
}

static void printBurst(FILE *f, const void *instance, void *env)
{
    const burst_t *b = (const burst_t *)instance;

    (void)env;
    for (uint8_t idx = 0u; idx < b->count; idx++)
    {
        fprintf(f, "%s+%" PRIu32 "ms:%02X", (idx != 0u) ? " " : "", b->gap[idx], b->byte[idx]);
    }
}

/* Write a byte at t=0, read at t=1: the same byte comes back */
static enum theft_trial_res propWriteReadRoundtrip(struct theft *t, void *arg)
{
    const uint8_t byte = *(const uint8_t *)arg;
    virtTmrCtx_t timer = { .timeNow = 0u };
    virtUartLinkCtx_t ctx = { .tmrHandle = &timer };
    uint8_t received = (uint8_t)~byte;

    (void)t;
    (void)UART_Init(&ctx);
    UART_WriteByte(&ctx, byte);
    timer.timeNow = 1u;
    if (OBD_UART_RX_OK != UART_RecvByte(&ctx, &received))
    {
        return THEFT_TRIAL_FAIL;
    }
    return (received == byte) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* A byte is not visible at the time it was written */
static enum theft_trial_res propNotReadyAtSameTime(struct theft *t, void *arg)
{
    const uint8_t byte = *(const uint8_t *)arg;
    virtTmrCtx_t timer = { .timeNow = 0u };
    virtUartLinkCtx_t ctx = { .tmrHandle = &timer };
    uint8_t received = 0u;

    (void)t;
    (void)UART_Init(&ctx);
    UART_WriteByte(&ctx, byte);
    return (OBD_UART_RX_NO_DATA == UART_RecvByte(&ctx, &received)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Every poll costs exactly one step, byte or not */
static enum theft_trial_res propPollCostsOneStep(struct theft *t, void *arg)
{
    const uint8_t byte = *(const uint8_t *)arg;
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = TIME_STEP };
    virtUartLinkCtx_t ctx = { .tmrHandle = &timer };
    uint8_t received = 0u;

    (void)t;
    (void)UART_Init(&ctx);
    UART_WriteByte(&ctx, byte);
    (void)UART_RecvByte(&ctx, &received);
    return (timer.timeNow == TIME_STEP) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* A burst written at increasing times is read back complete, in order, each byte only after it was
 * written, and nothing more */
static enum theft_trial_res propBurstInOrder(struct theft *t, void *arg)
{
    const burst_t *b = (const burst_t *)arg;
    virtTmrCtx_t timer = { .timeNow = 0u, .timeStep = 1u };
    virtUartLinkCtx_t ctx = { .tmrHandle = &timer };
    uint32_t writtenAt[MAX_BURST] = {0};
    uint32_t time = 0u;
    uint8_t received = 0u;
    uint8_t next = 0u;

    (void)t;
    (void)UART_Init(&ctx);
    for (uint8_t idx = 0u; idx < b->count; idx++)
    {
        time += b->gap[idx];
        writtenAt[idx] = time;
        timer.timeNow = time;
        UART_WriteByte(&ctx, b->byte[idx]);
    }

    /* poll from t=0 until well after the last byte */
    timer.timeNow = 0u;
    while (timer.timeNow <= (time + 2u * MAX_BURST))
    {
        uint32_t pollAt = timer.timeNow;
        obd_uart_rx_status_t st = UART_RecvByte(&ctx, &received);

        if (OBD_UART_RX_OK == st)
        {
            if ((next >= b->count) || (received != b->byte[next]) || (pollAt <= writtenAt[next]))
            {
                return THEFT_TRIAL_FAIL;
            }
            next++;
        }
        else if (OBD_UART_RX_NO_DATA != st)
        {
            return THEFT_TRIAL_FAIL;
        }
    }
    return (next == b->count) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       Any written byte is read back once time passes
 * - description: For any byte, write it at t=0, move the clock to
 *                t=1 and read it back unchanged.
 * - steps:
 *     1. Generate a byte, write it at t=0
 *     2. Read at t=1
 *     3. Read is OK with the same byte
 ******************************************************************/
void test_pos_pbt_UART_RecvByte_0(void)
{
    struct theft_run_config cfg = {
        .name      = "UART: a written byte is read back unchanged",
        .prop1     = propWriteReadRoundtrip,
        .type_info = { &byteInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_UART_RecvByte_0.vcd");
}

/******************************************************************
 * - brief:       A byte is never visible at its own timestamp
 * - description: For any byte written at t=0, a read while the clock
 *                is still 0 finds no data.
 * - steps:
 *     1. Generate a byte, write it at t=0
 *     2. Read at t=0 returns OBD_UART_RX_NO_DATA
 ******************************************************************/
void test_pos_pbt_UART_RecvByte_1(void)
{
    struct theft_run_config cfg = {
        .name      = "UART: a byte is not visible at its write time",
        .prop1     = propNotReadyAtSameTime,
        .type_info = { &byteInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_UART_RecvByte_1.vcd");
}

/******************************************************************
 * - brief:       Every RX poll moves the clock one step
 * - description: For any byte, reading advances the virtual clock by
 *                exactly timeStep.
 * - steps:
 *     1. Generate a byte, write it, read it
 *     2. The clock moved by timeStep
 ******************************************************************/
void test_pos_pbt_UART_RecvByte_2(void)
{
    struct theft_run_config cfg = {
        .name      = "UART: one poll costs one step",
        .prop1     = propPollCostsOneStep,
        .type_info = { &byteInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_UART_RecvByte_2.vcd");
}

/******************************************************************
 * - brief:       A burst of bytes is received complete and in order
 * - description: For any burst of 1..32 bytes written with 0..50 ms
 *                gaps, polling every ms returns exactly those bytes,
 *                in write order, each only after its write time.
 * - steps:
 *     1. Generate the bytes and the gaps, write them
 *     2. Poll every ms from t=0 until after the last byte
 *     3. Bytes come back in order, none early, none extra, none lost
 ******************************************************************/
void test_pos_pbt_UART_RecvByte_3(void)
{
    struct theft_run_config cfg = {
        .name      = "UART: a burst comes back complete and in order",
        .prop1     = propBurstInOrder,
        .type_info = { &burstInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_UART_RecvByte_3.vcd");
}
