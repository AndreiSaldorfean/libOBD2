/* ================================================ INCLUDES =============================================== */
#include "timer_pbt_test.h"
#include "libobd2_timer_port.h"
#include "pbt_support.h"
#include "theft.h"
#include "unity.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define MAX_DELAYS     16u
#define MAX_DELAY_MS   5000u
#define MAX_TIMEOUT_MS 10000u

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* A clock that starts somewhere and runs through a list of delays */
typedef struct
{
    uint32_t start;
    uint8_t  count;
    uint32_t delay[MAX_DELAYS];
} delayCase_t;

/* A timeout armed at 'start' for 'timeout' ms, probed 'probe' ms later */
typedef struct
{
    uint32_t start;
    uint32_t timeout;
    uint32_t probe;
} timeoutCase_t;

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static enum theft_alloc_res allocDelayCase(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocTimeoutCase(struct theft *t, void *env, void **output);
static void printDelayCase(FILE *f, const void *instance, void *env);
static void printTimeoutCase(FILE *f, const void *instance, void *env);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info delayCaseInfo   = PBT_TYPE_INFO(allocDelayCase, printDelayCase);
static struct theft_type_info timeoutCaseInfo = PBT_TYPE_INFO(allocTimeoutCase, printTimeoutCase);

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* The start stays clear of the 32-bit wrap: the virtual port is a test double and does not claim to
 * handle it (the real ports compare start + duration, which does) */
static enum theft_alloc_res allocDelayCase(struct theft *t, void *env, void **output)
{
    delayCase_t *c = calloc(1u, sizeof(*c));

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->start = PBT_Range(t, 0u, UINT32_MAX / 2u);
    c->count = (uint8_t)PBT_Range(t, 0u, MAX_DELAYS);
    for (uint8_t idx = 0u; idx < c->count; idx++)
    {
        c->delay[idx] = PBT_Range(t, 0u, MAX_DELAY_MS);
    }
    *output = c;
    return THEFT_ALLOC_OK;
}

static enum theft_alloc_res allocTimeoutCase(struct theft *t, void *env, void **output)
{
    timeoutCase_t *c = calloc(1u, sizeof(*c));

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->start   = PBT_Range(t, 0u, UINT32_MAX / 2u);
    c->timeout = PBT_Range(t, 0u, MAX_TIMEOUT_MS);
    c->probe   = PBT_Range(t, 0u, 2u * MAX_TIMEOUT_MS);
    *output = c;
    return THEFT_ALLOC_OK;
}

static void printDelayCase(FILE *f, const void *instance, void *env)
{
    const delayCase_t *c = (const delayCase_t *)instance;

    (void)env;
    fprintf(f, "start=%" PRIu32 " delays=[", c->start);
    for (uint8_t idx = 0u; idx < c->count; idx++)
    {
        fprintf(f, "%s%" PRIu32, (idx != 0u) ? ", " : "", c->delay[idx]);
    }
    fprintf(f, "]");
}

static void printTimeoutCase(FILE *f, const void *instance, void *env)
{
    const timeoutCase_t *c = (const timeoutCase_t *)instance;

    (void)env;
    fprintf(f, "start=%" PRIu32 " timeout=%" PRIu32 " probe=+%" PRIu32, c->start, c->timeout, c->probe);
}

/* Delays add up exactly, and neither the step nor a running timeout is touched */
static enum theft_trial_res propDelaysAddUp(struct theft *t, void *arg)
{
    const delayCase_t *c = (const delayCase_t *)arg;
    virtTmrCtx_t timer = { .timeNow = c->start, .timeStep = 10u };
    uint64_t expected = c->start;

    (void)t;
    if (OBD_TMR_OK != TMR_StartTimeout(&timer, MAX_TIMEOUT_MS, NULL, NULL))
    {
        return THEFT_TRIAL_FAIL;
    }
    for (uint8_t idx = 0u; idx < c->count; idx++)
    {
        TMR_DelayMs(&timer, c->delay[idx]);
        expected += c->delay[idx];
        if (TMR_GetTimeMs(&timer) != (uint32_t)expected)
        {
            return THEFT_TRIAL_FAIL;
        }
    }
    return ((timer.timeStep == 10u) && (timer.timeoutTime == (c->start + MAX_TIMEOUT_MS)))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Expired exactly when the probe is past the deadline; a stop always disarms */
static enum theft_trial_res propTimeoutWindow(struct theft *t, void *arg)
{
    const timeoutCase_t *c = (const timeoutCase_t *)arg;
    virtTmrCtx_t timer = { .timeNow = c->start, .timeStep = 10u };
    bool expired = false;

    (void)t;
    if (OBD_TMR_OK != TMR_StartTimeout(&timer, c->timeout, NULL, NULL))
    {
        return THEFT_TRIAL_FAIL;
    }
    TMR_DelayMs(&timer, c->probe);
    expired = TMR_IsTimeoutExpired(&timer);
    if (expired != (c->probe > c->timeout))
    {
        return THEFT_TRIAL_FAIL;
    }
    if ((OBD_TMR_OK != TMR_StopTimeout(&timer)) || TMR_IsTimeoutExpired(&timer))
    {
        return THEFT_TRIAL_FAIL;
    }
    return THEFT_TRIAL_PASS;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       Delays add up exactly
 * - description: For any start time and list of delays the virtual
 *                clock is the start plus the sum of the delays after
 *                every step; the poll step and an armed timeout are
 *                never touched by a delay.
 * - steps:
 *     1. Generate a start time and up to 16 delays of 0..5000 ms
 *     2. Arm a timeout, apply the delays one by one
 *     3. After each delay the clock equals start + sum so far
 *     4. Step and deadline are unchanged
 ******************************************************************/
void test_pos_pbt_TMR_DelayMs_0(void)
{
    struct theft_run_config cfg = {
        .name      = "TMR_DelayMs: delays add up, nothing else changes",
        .prop1     = propDelaysAddUp,
        .type_info = { &delayCaseInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}

/******************************************************************
 * - brief:       A timeout expires exactly after its deadline
 * - description: For any start, timeout length and probe time,
 *                IsTimeoutExpired is true exactly when the probe is
 *                past start + timeout, and false again once stopped.
 * - steps:
 *     1. Generate start, timeout (0..10 s) and probe offset (0..20 s)
 *     2. Arm the timeout, move the clock by the probe offset
 *     3. Expired == (probe > timeout)
 *     4. After StopTimeout it is not expired
 ******************************************************************/
void test_pos_pbt_TMR_IsTimeoutExpired_0(void)
{
    struct theft_run_config cfg = {
        .name      = "TMR_IsTimeoutExpired: expired iff past the deadline",
        .prop1     = propTimeoutWindow,
        .type_info = { &timeoutCaseInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}
