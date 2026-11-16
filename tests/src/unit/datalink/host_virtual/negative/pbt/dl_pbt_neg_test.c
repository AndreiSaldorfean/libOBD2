/* ================================================ INCLUDES =============================================== */
#include "dl_pbt_neg_test.h"
#include "dl_fixture.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "pbt_support.h"
#include "statusRetCodes.h"
#include "theft.h"
#include "unity.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define MAX_START  100000u
#define MAX_STEP   10u
#define MAX_WINDOW 300u
#define NO_BYTE    UINT32_MAX /* windowCase_t.arrival: nothing is sent */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint32_t             start;
    uint32_t             step;
    uint16_t             timeMin;
    uint16_t             timeMax;
    uint32_t             arrival; /* ms after the start, or NO_BYTE */
    uint8_t              byte;
    obd_uart_rx_status_t error;   /* rx error the byte arrives with */
} windowCase_t;

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static enum theft_alloc_res allocMissed(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocEarly(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocBroken(struct theft *t, void *env, void **output);
static void printWindowCase(FILE *f, const void *instance, void *env);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info missedInfo = PBT_TYPE_INFO(allocMissed, printWindowCase);
static struct theft_type_info earlyInfo  = PBT_TYPE_INFO(allocEarly, printWindowCase);
static struct theft_type_info brokenInfo = PBT_TYPE_INFO(allocBroken, printWindowCase);

static const obd_uart_rx_status_t rxErrors[] =
{
    OBD_UART_RX_PARITY_ERR, OBD_UART_RX_FRAMING_ERR, OBD_UART_RX_OVERRUN_ERR, OBD_UART_RX_NOISE_ERR,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static windowCase_t *newCase(struct theft *t)
{
    windowCase_t *c = calloc(1u, sizeof(*c));

    if (c != NULL)
    {
        c->start   = PBT_Range(t, 0u, MAX_START);
        c->step    = PBT_Range(t, 1u, MAX_STEP);
        c->byte    = PBT_Byte(t);
        c->error   = OBD_UART_RX_OK;
        c->arrival = NO_BYTE;
    }
    return c;
}

/* No byte at all, or one that lands after timeMax */
static enum theft_alloc_res allocMissed(struct theft *t, void *env, void **output)
{
    windowCase_t *c = newCase(t);

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->timeMin = (uint16_t)PBT_Range(t, 0u, MAX_WINDOW / 2u);
    c->timeMax = (uint16_t)PBT_Range(t, c->timeMin, MAX_WINDOW);
    if (!PBT_OneIn(t, 2u))
    {
        c->arrival = PBT_Range(t, c->timeMax + c->step, c->timeMax + 4u * MAX_STEP);
    }
    *output = c;
    return THEFT_ALLOC_OK;
}

/* A byte that is read before timeMin: it lands at least two polls before timeMin */
static enum theft_alloc_res allocEarly(struct theft *t, void *env, void **output)
{
    windowCase_t *c = newCase(t);

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->timeMin = (uint16_t)PBT_Range(t, 3u * c->step, MAX_WINDOW / 2u);
    c->timeMax = (uint16_t)PBT_Range(t, c->timeMin, MAX_WINDOW);
    c->arrival = PBT_Range(t, 0u, c->timeMin - 2u * c->step - 1u);
    *output = c;
    return THEFT_ALLOC_OK;
}

/* A byte inside the window that arrives with an rx error */
static enum theft_alloc_res allocBroken(struct theft *t, void *env, void **output)
{
    windowCase_t *c = newCase(t);

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->timeMin = 0u;
    c->timeMax = (uint16_t)PBT_Range(t, c->step, MAX_WINDOW);
    c->arrival = PBT_Range(t, 0u, c->timeMax - c->step);
    c->error   = rxErrors[PBT_Range(t, 0u, (sizeof(rxErrors) / sizeof(rxErrors[0])) - 1u)];
    *output = c;
    return THEFT_ALLOC_OK;
}

static void printWindowCase(FILE *f, const void *instance, void *env)
{
    const windowCase_t *c = (const windowCase_t *)instance;

    (void)env;
    fprintf(f, "start=%" PRIu32 " step=%" PRIu32 " window=[%u, %u] ", c->start, c->step, c->timeMin, c->timeMax);
    if (c->arrival == NO_BYTE)
    {
        fprintf(f, "no byte");
    }
    else
    {
        fprintf(f, "byte 0x%02X at +%" PRIu32 "ms, rx status %u", c->byte, c->arrival, (unsigned)c->error);
    }
}

/* onWrite hook: the byte arrives with the rx error in pUserData */
static void injectError(void *pUserData, uartEvent_t *event)
{
    event->error = *(const obd_uart_rx_status_t *)pUserData;
}

/* Puts the case on the dl_fixture line and runs DL_ReadByteInTimeframe */
static obd_status_t runRead(const windowCase_t *c, uint8_t *received, uint32_t *elapsed)
{
    DLFX_SetUpLine();
    dlFx.timer.timeNow  = c->start;
    dlFx.timer.timeStep = c->step;
    if (c->arrival != NO_BYTE)
    {
        dlFx.uart.faults.onWrite   = injectError;
        dlFx.uart.faults.pUserData = (void *)&c->error;
        DLFX_LineWriteAt(c->start + c->arrival, c->byte);
        dlFx.uart.faults.onWrite = NULL;
    }
    return DL_ReadByteInTimeframe(&dlFx.link, elapsed, received, c->timeMin, c->timeMax);
}

/* Missed byte: max timeout, no error, gives up within one poll after timeMax, a late byte stays on
 * the line for the next read */
static enum theft_trial_res propMissedTimesOut(struct theft *t, void *arg)
{
    const windowCase_t *c = (const windowCase_t *)arg;
    uint8_t received = (uint8_t)~c->byte;
    uint32_t took = 0u;
    obd_status_t status = {0};

    (void)t;
    status = runRead(c, &received, NULL);
    took = dlFx.timer.timeNow - c->start;
    if ((status.timeout != OBD_ERR_TIMEOUT_MAX) || (status.response != OBD_STATUS_OK)
     || (took <= c->timeMax) || (took > (c->timeMax + c->step)))
    {
        return THEFT_TRIAL_FAIL;
    }
    if (c->arrival != NO_BYTE)
    {
        dlFx.timer.timeNow = c->start + c->arrival + 1u;
        if ((OBD_UART_RX_OK != UART_RecvByte(&dlFx.uart, &received)) || (received != c->byte))
        {
            return THEFT_TRIAL_FAIL;
        }
    }
    return THEFT_TRIAL_PASS;
}

/* Missed byte: the elapsed time reported is the time the call took */
static enum theft_trial_res propMissedElapsed(struct theft *t, void *arg)
{
    const windowCase_t *c = (const windowCase_t *)arg;
    uint8_t received = 0u;
    uint32_t elapsed = 0u;

    (void)t;
    (void)runRead(c, &received, &elapsed);
    return (elapsed == (dlFx.timer.timeNow - c->start)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Early byte: min timeout, byte handed over, elapsed below timeMin */
static enum theft_trial_res propEarlyFlagged(struct theft *t, void *arg)
{
    const windowCase_t *c = (const windowCase_t *)arg;
    uint8_t received = (uint8_t)~c->byte;
    uint32_t elapsed = 0u;
    obd_status_t status = {0};

    (void)t;
    status = runRead(c, &received, &elapsed);
    return ((status.timeout == OBD_ERR_TIMEOUT_MIN) && (received == c->byte) && (elapsed < c->timeMin))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Broken byte: the matching UART error in .response, never OK */
static enum theft_trial_res propBrokenReported(struct theft *t, void *arg)
{
    const windowCase_t *c = (const windowCase_t *)arg;
    uint8_t received = 0u;
    obd_status_t status = {0};

    (void)t;
    status = runRead(c, &received, NULL);
    return ((status.response == DL_UartRxStatusToResponse(c->error)) && (status.response != OBD_STATUS_OK)
         && (status.timeout == OBD_STATUS_OK)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Broken byte: RecvByteBlocking does not hand it out as good */
static enum theft_trial_res propBlockingRejectsBroken(struct theft *t, void *arg)
{
    const windowCase_t *c = (const windowCase_t *)arg;
    uint8_t received = 0u;

    (void)t;
    DLFX_SetUpLine();
    dlFx.timer.timeStep        = c->step;
    dlFx.uart.faults.onWrite   = injectError;
    dlFx.uart.faults.pUserData = (void *)&c->error;
    DLFX_LineWriteAt(c->arrival, c->byte);
    dlFx.uart.faults.onWrite = NULL;
    return DL_RecvByteBlocking(&dlFx.link, &received) ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A missed byte ends in a max timeout, nothing lost
 * - description: For no byte, or a byte landing after timeMax, the
 *                read reports OBD_ERR_TIMEOUT_MAX with no UART error,
 *                gives up within one poll after timeMax, and leaves a
 *                late byte on the line for the next read.
 * - steps:
 *     1. Generate start, step, window and no byte / a late byte
 *     2. DL_ReadByteInTimeframe: TIMEOUT_MAX, response OK
 *     3. timeMax < time taken <= timeMax + step
 *     4. The late byte is still readable afterwards
 ******************************************************************/
void test_neg_pbt_DL_ReadByteInTimeframe_0(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_ReadByteInTimeframe: a missed byte times out cleanly",
        .prop1     = propMissedTimesOut,
        .type_info = { &missedInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_DL_ReadByteInTimeframe_0.vcd");
}

/******************************************************************
 * - brief:       A timed-out read reports the time it took
 * - description: The elapsed time handed back must be the time the
 *                call really took, on the timeout path too: callers
 *                use it to budget the rest of a protocol window.
 * - steps:
 *     1. Generate start, step, window and no byte / a late byte
 *     2. DL_ReadByteInTimeframe times out
 *     3. *timeElapsed == clock after - clock before
 ******************************************************************/
void test_neg_pbt_DL_ReadByteInTimeframe_1(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_ReadByteInTimeframe: a timeout reports the time taken",
        .prop1     = propMissedElapsed,
        .type_info = { &missedInfo },
    };
    PBT_KNOWN_BUG(&cfg, "test_neg_pbt_DL_ReadByteInTimeframe_1.vcd",
                  "F2: on a timeout *timeElapsed is 0 - timeStart, not the time taken (PROPERTY_TESTS.md)");
}

/******************************************************************
 * - brief:       A byte before timeMin is flagged as too early
 * - description: A byte read before the window opens is handed back
 *                but reported with OBD_ERR_TIMEOUT_MIN, and the
 *                elapsed time shows why.
 * - steps:
 *     1. Generate a window and a byte two polls or more before timeMin
 *     2. DL_ReadByteInTimeframe: TIMEOUT_MIN, the byte, elapsed < timeMin
 ******************************************************************/
void test_neg_pbt_DL_ReadByteInTimeframe_2(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_ReadByteInTimeframe: an early byte is flagged",
        .prop1     = propEarlyFlagged,
        .type_info = { &earlyInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_DL_ReadByteInTimeframe_2.vcd");
}

/******************************************************************
 * - brief:       A byte with a UART error is never reported OK
 * - description: For any byte inside the window that arrives with a
 *                parity, framing, overrun or noise error, the read
 *                reports that error in .response, timeout OK.
 * - steps:
 *     1. Generate a window, a byte inside it and an rx error
 *     2. .response == the mapped UART error, never OK; .timeout OK
 ******************************************************************/
void test_neg_pbt_DL_ReadByteInTimeframe_3(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_ReadByteInTimeframe: a broken byte reports its error",
        .prop1     = propBrokenReported,
        .type_info = { &brokenInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_DL_ReadByteInTimeframe_3.vcd");
}

/******************************************************************
 * - brief:       RecvByteBlocking rejects a broken byte
 * - description: For any byte that arrives with an rx error, the
 *                blocking read returns false instead of handing it
 *                out as a good byte.
 * - steps:
 *     1. Generate a byte, its arrival and an rx error
 *     2. DL_RecvByteBlocking returns false
 ******************************************************************/
void test_neg_pbt_DL_RecvByteBlocking_0(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_RecvByteBlocking: a broken byte is not good",
        .prop1     = propBlockingRejectsBroken,
        .type_info = { &brokenInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_DL_RecvByteBlocking_0.vcd");
}
