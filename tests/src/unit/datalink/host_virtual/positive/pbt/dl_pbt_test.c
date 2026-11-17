/* ================================================ INCLUDES =============================================== */
#include "dl_pbt_test.h"
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
#define MAX_START   100000u /* clock value when the read starts */
#define MAX_STEP    10u     /* ms per UART poll */
#define MAX_WINDOW  300u    /* largest timeMax generated (W1 max) */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* One DL_ReadByteInTimeframe call: clock 'start', poll step, window [timeMin, timeMax] and a byte that
 * lands 'arrival' ms after the start */
typedef struct
{
    uint32_t start;
    uint32_t step;
    uint16_t timeMin;
    uint16_t timeMax;
    uint32_t arrival;
    uint8_t  byte;
} windowCase_t;

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static enum theft_alloc_res allocInWindow(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocByte(struct theft *t, void *env, void **output);
static void printWindowCase(FILE *f, const void *instance, void *env);
static void printByte(FILE *f, const void *instance, void *env);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info inWindowInfo = PBT_TYPE_INFO(allocInWindow, printWindowCase);
static struct theft_type_info byteInfo     = PBT_TYPE_INFO(allocByte, printByte);

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* A byte that arrives inside the window: no earlier than timeMin, and at least one poll before timeMax
 * (a polling receiver needs one poll to see it) */
static enum theft_alloc_res allocInWindow(struct theft *t, void *env, void **output)
{
    windowCase_t *c = calloc(1u, sizeof(*c));

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->start   = PBT_Range(t, 0u, MAX_START);
    c->step    = PBT_Range(t, 1u, MAX_STEP);
    c->timeMin = (uint16_t)PBT_Range(t, 0u, MAX_WINDOW / 2u);
    c->timeMax = (uint16_t)PBT_Range(t, c->timeMin + c->step, MAX_WINDOW);
    c->arrival = PBT_Range(t, c->timeMin, c->timeMax - c->step);
    c->byte    = PBT_Byte(t);
    *output = c;
    return THEFT_ALLOC_OK;
}

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

static void printWindowCase(FILE *f, const void *instance, void *env)
{
    const windowCase_t *c = (const windowCase_t *)instance;

    (void)env;
    fprintf(f, "start=%" PRIu32 " step=%" PRIu32 " window=[%u, %u] byte 0x%02X at +%" PRIu32 "ms",
            c->start, c->step, c->timeMin, c->timeMax, c->byte, c->arrival);
}

static void printByte(FILE *f, const void *instance, void *env)
{
    (void)env;
    fprintf(f, "0x%02X", *(const uint8_t *)instance);
}

/* Fresh line with the clock at 'start' and the given poll step */
static void setUpLine(uint32_t start, uint32_t step)
{
    DLFX_SetUpLine();
    dlFx.timer.timeNow  = start;
    dlFx.timer.timeStep = step;
}

/* A byte inside the window is returned OK, unchanged, and the reported elapsed time is the time the
 * call really took and lies inside [timeMin, timeMax + step] */
static enum theft_trial_res propInWindow(struct theft *t, void *arg)
{
    const windowCase_t *c = (const windowCase_t *)arg;
    uint8_t received = (uint8_t)~c->byte;
    uint32_t elapsed = 0u;
    obd_status_t status = {0};

    (void)t;
    setUpLine(c->start, c->step);
    DLFX_LineWriteAt(c->start + c->arrival, c->byte);
    status = DL_ReadByteInTimeframe(&dlFx.link, &elapsed, &received, c->timeMin, c->timeMax);

    if ((status.response != OBD_STATUS_OK) || (status.timeout != OBD_STATUS_OK) || (received != c->byte))
    {
        return THEFT_TRIAL_FAIL;
    }
    if ((elapsed != (dlFx.timer.timeNow - c->start)) || (elapsed < c->timeMin) || (elapsed > (c->timeMax + c->step)))
    {
        return THEFT_TRIAL_FAIL;
    }
    return THEFT_TRIAL_PASS;
}

/* A byte already on the line is returned by a [0, step+] window */
static enum theft_trial_res propQueuedByteReturned(struct theft *t, void *arg)
{
    const uint8_t byte = *(const uint8_t *)arg;
    uint8_t received = (uint8_t)~byte;
    obd_status_t status = {0};

    (void)t;
    setUpLine(0u, MAX_STEP);
    UART_WriteByte(&dlFx.uart, byte);
    status = DL_ReadByteInTimeframe(&dlFx.link, NULL, &received, 0u, 2u * MAX_STEP);
    return ((status.response == OBD_STATUS_OK) && (status.timeout == OBD_STATUS_OK) && (received == byte))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* An empty line ends in a max timeout, without a byte made up from an empty queue slot */
static enum theft_trial_res propEmptyLineTimesOut(struct theft *t, void *arg)
{
    const uint8_t marker = *(const uint8_t *)arg;
    uint8_t received = marker;
    obd_status_t status = {0};

    (void)t;
    setUpLine(0u, MAX_STEP);
    status = DL_ReadByteInTimeframe(&dlFx.link, NULL, &received, 0u, (uint16_t)(marker % MAX_WINDOW));
    return ((status.timeout == OBD_ERR_TIMEOUT_MAX) && (received == marker)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* RecvByteBlocking returns a byte that is on the line */
static enum theft_trial_res propBlockingReturnsByte(struct theft *t, void *arg)
{
    const uint8_t byte = *(const uint8_t *)arg;
    uint8_t received = (uint8_t)~byte;

    (void)t;
    setUpLine(0u, MAX_STEP);
    DLFX_LineWriteAt(byte * 4u, byte);
    return (DL_RecvByteBlocking(&dlFx.link, &received) && (received == byte)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A queued byte is returned inside its window
 * - description: For any byte already on the line, a read with a
 *                window wider than one poll returns it with OK.
 * - steps:
 *     1. Generate a byte, put it on the line
 *     2. DL_ReadByteInTimeframe(0, 2 steps) returns OK and the byte
 ******************************************************************/
void test_pos_pbt_DL_ReadByteInTimeframe_0(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_ReadByteInTimeframe: a queued byte is returned",
        .prop1     = propQueuedByteReturned,
        .type_info = { &byteInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_DL_ReadByteInTimeframe_0.vcd");
}

/******************************************************************
 * - brief:       No byte means a max timeout, never a hang
 * - description: With nothing on the line the read gives up with
 *                OBD_ERR_TIMEOUT_MAX and leaves the buffer alone.
 * - steps:
 *     1. Generate a window, leave the line empty
 *     2. DL_ReadByteInTimeframe reports OBD_ERR_TIMEOUT_MAX
 *     3. The receive buffer is untouched
 ******************************************************************/
void test_pos_pbt_DL_ReadByteInTimeframe_1(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_ReadByteInTimeframe: an empty line times out",
        .prop1     = propEmptyLineTimesOut,
        .type_info = { &byteInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_DL_ReadByteInTimeframe_1.vcd");
}

/******************************************************************
 * - brief:       A byte inside the window is read, and timed right
 * - description: For any start time, poll step, window and arrival
 *                time inside the window, the read returns the byte
 *                with OK, and the elapsed time it reports is the time
 *                the call took, within [timeMin, timeMax + step].
 * - steps:
 *     1. Generate start, step, window and arrival (timeMin..timeMax-step)
 *     2. Put the byte on the line, read
 *     3. OK, same byte
 *     4. elapsed == clock advance, timeMin <= elapsed <= timeMax + step
 ******************************************************************/
void test_pos_pbt_DL_ReadByteInTimeframe_2(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_ReadByteInTimeframe: in-window byte is OK and timed right",
        .prop1     = propInWindow,
        .type_info = { &inWindowInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_DL_ReadByteInTimeframe_2.vcd");
}

/******************************************************************
 * - brief:       RecvByteBlocking returns a byte on the line
 * - description: For any byte arriving within the first second, the
 *                blocking read keeps polling until it gets it.
 * - steps:
 *     1. Generate a byte, put it on the line at 0..1020 ms
 *     2. DL_RecvByteBlocking returns true with the byte
 ******************************************************************/
void test_pos_pbt_DL_RecvByteBlocking_0(void)
{
    struct theft_run_config cfg = {
        .name      = "DL_RecvByteBlocking: returns the byte on the line",
        .prop1     = propBlockingReturnsByte,
        .type_info = { &byteInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_DL_RecvByteBlocking_0.vcd");
}
