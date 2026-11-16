/* ================================================ INCLUDES =============================================== */
#include "iso9141_pbt_neg_test.h"
#include "dl_fixture.h"
#include "dl_pbt_support.h"
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
#define NEGATIVE_RESPONSE_SID 0x7Fu
#define FRAME_DATA_CAP        12u /* sizeof(obd_data_t): the most a receive buffer can hold */
#define LATE_SPAN             40u /* how far past a window a late byte lands */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* An ECU response plus what was done to it */
typedef struct
{
    dlFxWire_t wire;
    uint8_t    index; /* byte that was changed / delayed */
    uint8_t    was;   /* its value before the change */
} badResponse_t;

/* One way the ECU side of a 5-baud init goes wrong */
typedef enum
{
    INIT_SYNC_EARLY = 0,
    INIT_SYNC_LATE,
    INIT_KB1_EARLY,
    INIT_KB1_LATE,
    INIT_KB2_LATE,
    INIT_INV_EARLY,
    INIT_INV_LATE,
    INIT_WRONG_SYNC,
    INIT_WRONG_KB,
    INIT_WRONG_INV,
    INIT_DEVIATIONS,
} initDeviation_t;

typedef struct
{
    dlFxInit_t      ecu;
    initDeviation_t deviation;
} badInit_t;

/* A request length send_request must refuse */
typedef struct
{
    size_t     len;
    obd_data_t data;
} badRequest_t;

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static enum theft_alloc_res allocCorrupted(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocWrongHeader(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocTooLong(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocStalled(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocEarly(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocRejected(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocBadInit(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocBadRequest(struct theft *t, void *env, void **output);
static void printBadResponse(FILE *f, const void *instance, void *env);
static void printBadInit(FILE *f, const void *instance, void *env);
static void printBadRequest(FILE *f, const void *instance, void *env);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info corruptedInfo   = PBT_TYPE_INFO(allocCorrupted, printBadResponse);
static struct theft_type_info wrongHeaderInfo = PBT_TYPE_INFO(allocWrongHeader, printBadResponse);
static struct theft_type_info tooLongInfo     = PBT_TYPE_INFO(allocTooLong, printBadResponse);
static struct theft_type_info stalledInfo     = PBT_TYPE_INFO(allocStalled, printBadResponse);
static struct theft_type_info earlyInfo       = PBT_TYPE_INFO(allocEarly, printBadResponse);
static struct theft_type_info rejectedInfo    = PBT_TYPE_INFO(allocRejected, printBadResponse);
static struct theft_type_info badInitInfo     = PBT_TYPE_INFO(allocBadInit, printBadInit);
static struct theft_type_info badRequestInfo  = PBT_TYPE_INFO(allocBadRequest, printBadRequest);

static const char *const deviationNames[INIT_DEVIATIONS] =
{
    "sync early", "sync late", "KB1 early", "KB1 late", "KB2 late", "inv addr early", "inv addr late",
    "wrong sync", "wrong keyword bytes", "wrong inv addr",
};

/* What ISO9141_Connect must report for each deviation */
static const uint16_t deviationError[INIT_DEVIATIONS] =
{
    OBD_ERR_5BAUD_SYNC_NOT_RECVD, OBD_ERR_5BAUD_SYNC_NOT_RECVD, OBD_ERR_5BAUD_KB1_NOT_RECVD,
    OBD_ERR_5BAUD_KB1_NOT_RECVD, OBD_ERR_5BAUD_KB2_NOT_RECVD, OBD_ERR_5BAUD_INV_ADDR_NOT_RECVD,
    OBD_ERR_5BAUD_INV_ADDR_NOT_RECVD, OBD_ERR_5BAUD_WRONG_SYNC_BYTE, OBD_ERR_5BAUD_WRONG_KEYBYTES,
    OBD_ERR_5BAUD_WRONG_INV_ADDR,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static badResponse_t *newResponse(struct theft *t, uint8_t minData, uint8_t maxData, int sid)
{
    badResponse_t *r = calloc(1u, sizeof(*r));

    if (r != NULL)
    {
        DLFX_PBT_GenResponse(t, &r->wire, (uint8_t)PBT_Range(t, minData, maxData), sid);
    }
    return r;
}

/* One byte changed, without a UART error. Half the time it is changed to the sum of the bytes before
 * it: the corruption a checksum-framed receiver is most likely to miss. */
static enum theft_alloc_res allocCorrupted(struct theft *t, void *env, void **output)
{
    badResponse_t *r = newResponse(t, 1u, DLFX_PBT_MAX_DATA, -1);
    uint8_t runningSum = 0u;

    (void)env;
    if (r == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    r->index = (uint8_t)PBT_Range(t, 0u, r->wire.len - 1u);
    r->was   = r->wire.byte[r->index];
    runningSum = DLFX_PBT_Checksum(r->wire.byte, r->index);
    if (PBT_OneIn(t, 2u) && (r->index >= DLFX_PBT_HEADER_LEN) && (runningSum != r->was))
    {
        r->wire.byte[r->index] = runningSum;
    }
    else
    {
        r->wire.byte[r->index] = (uint8_t)(r->was ^ (uint8_t)PBT_Range(t, 1u, 0xFFu));
    }
    *output = r;
    return THEFT_ALLOC_OK;
}

/* Format or target byte not the ISO 9141-2 response header 48 6B, checksum recomputed */
static enum theft_alloc_res allocWrongHeader(struct theft *t, void *env, void **output)
{
    badResponse_t *r = newResponse(t, 1u, DLFX_PBT_MAX_DATA, -1);

    (void)env;
    if (r == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    r->index = (uint8_t)PBT_Range(t, 0u, 1u);
    r->was   = r->wire.byte[r->index];
    r->wire.byte[r->index] = (uint8_t)(r->was ^ (uint8_t)PBT_Range(t, 1u, 0xFFu));
    r->wire.byte[r->wire.len - 1u] = DLFX_PBT_Checksum(r->wire.byte, r->wire.len - 1u);
    *output = r;
    return THEFT_ALLOC_OK;
}

/* 8..12 data bytes: longer than ISO 9141-2 allows, still fits the receive buffer */
static enum theft_alloc_res allocTooLong(struct theft *t, void *env, void **output)
{
    badResponse_t *r = newResponse(t, DLFX_PBT_MAX_DATA + 1u, FRAME_DATA_CAP, -1);

    (void)env;
    if (r == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    *output = r;
    return THEFT_ALLOC_OK;
}

/* The ECU pauses longer than P1 max before one byte */
static enum theft_alloc_res allocStalled(struct theft *t, void *env, void **output)
{
    badResponse_t *r = newResponse(t, 1u, DLFX_PBT_MAX_DATA, -1);
    uint32_t pause = 0u;

    (void)env;
    if (r == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    r->index = (uint8_t)PBT_Range(t, 1u, r->wire.len - 1u);
    pause = P1_TIME_MAX + PBT_Range(t, 2u, LATE_SPAN);
    for (uint8_t idx = r->index; idx < r->wire.len; idx++)
    {
        r->wire.at[idx] += pause;
    }
    *output = r;
    return THEFT_ALLOC_OK;
}

/* The format byte lands before P2 min */
static enum theft_alloc_res allocEarly(struct theft *t, void *env, void **output)
{
    badResponse_t *r = newResponse(t, 1u, DLFX_PBT_MAX_DATA, -1);
    uint32_t shift = 0u;

    (void)env;
    if (r == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    shift = r->wire.at[0] - PBT_Range(t, 0u, P2_TIME_MIN - 3u);
    for (uint8_t idx = 0u; idx < r->wire.len; idx++)
    {
        r->wire.at[idx] -= shift;
    }
    *output = r;
    return THEFT_ALLOC_OK;
}

/* A negative response: sid 7F, any rest */
static enum theft_alloc_res allocRejected(struct theft *t, void *env, void **output)
{
    badResponse_t *r = newResponse(t, 1u, DLFX_PBT_MAX_DATA, NEGATIVE_RESPONSE_SID);

    (void)env;
    if (r == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    *output = r;
    return THEFT_ALLOC_OK;
}

/* A correct ECU answer with exactly one thing wrong */
static enum theft_alloc_res allocBadInit(struct theft *t, void *env, void **output)
{
    badInit_t *b = calloc(1u, sizeof(*b));
    dlFxInit_t *e = NULL;

    (void)env;
    if (b == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    e = &b->ecu;
    e->sync    = 0x55u;
    e->kb1     = PBT_OneIn(t, 2u) ? 0x94u : 0x08u;
    e->kb2     = e->kb1;
    e->invAddr = (uint8_t)~DLFX_TARGET_ADDR;
    e->w1 = PBT_Range(t, ISO9141_W1_TIME_MIN, ISO9141_W1_TIME_MAX - 1u);
    e->w2 = PBT_Range(t, ISO9141_W2_TIME_MIN, ISO9141_W2_TIME_MAX - 1u);
    e->w3 = PBT_Range(t, ISO9141_W3_TIME_MIN, ISO9141_W3_TIME_MAX - 1u);
    e->w4 = PBT_Range(t, ISO9141_W4_TIME_MIN, ISO9141_W4_TIME_MAX - 2u);

    b->deviation = (initDeviation_t)PBT_Range(t, 0u, INIT_DEVIATIONS - 1u);
    switch (b->deviation)
    {
        case INIT_SYNC_EARLY: e->w1 = PBT_Range(t, 0u, ISO9141_W1_TIME_MIN - 3u); break;
        case INIT_SYNC_LATE:  e->w1 = PBT_Range(t, ISO9141_W1_TIME_MAX + 1u, ISO9141_W1_TIME_MAX + LATE_SPAN); break;
        case INIT_KB1_EARLY:  e->w2 = PBT_Range(t, 0u, ISO9141_W2_TIME_MIN - 1u); break;
        case INIT_KB1_LATE:   e->w2 = PBT_Range(t, ISO9141_W2_TIME_MAX + 2u, ISO9141_W2_TIME_MAX + LATE_SPAN); break;
        case INIT_KB2_LATE:   e->w3 = PBT_Range(t, ISO9141_W3_TIME_MAX + 2u, ISO9141_W3_TIME_MAX + LATE_SPAN); break;
        case INIT_INV_EARLY:  e->w4 = PBT_Range(t, 0u, ISO9141_W4_TIME_MIN - 3u); break;
        case INIT_INV_LATE:   e->w4 = PBT_Range(t, ISO9141_W4_TIME_MAX + 1u, ISO9141_W4_TIME_MAX + LATE_SPAN); break;
        case INIT_WRONG_SYNC: e->sync = (uint8_t)(0x55u ^ (uint8_t)PBT_Range(t, 1u, 0xFFu)); break;
        case INIT_WRONG_INV:  e->invAddr = (uint8_t)(e->invAddr ^ (uint8_t)PBT_Range(t, 1u, 0xFFu)); break;
        default: /* INIT_WRONG_KB: any pair but 08 08 and 94 94 */
            e->kb1 = PBT_Byte(t);
            e->kb2 = PBT_Byte(t);
            if ((e->kb1 == e->kb2) && ((e->kb1 == 0x08u) || (e->kb1 == 0x94u)))
            {
                e->kb2 = (uint8_t)~e->kb2;
            }
            break;
    }
    *output = b;
    return THEFT_ALLOC_OK;
}

static enum theft_alloc_res allocBadRequest(struct theft *t, void *env, void **output)
{
    badRequest_t *r = calloc(1u, sizeof(*r));
    uint8_t *raw = NULL;

    (void)env;
    if (r == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    r->len = PBT_OneIn(t, 4u) ? 0u : PBT_Range(t, DLFX_PBT_MAX_DATA + 1u, 0xFFFFu);
    raw = (uint8_t *)&r->data;
    for (size_t idx = 0u; idx < sizeof(r->data); idx++)
    {
        raw[idx] = PBT_Byte(t);
    }
    *output = r;
    return THEFT_ALLOC_OK;
}

static void printBadResponse(FILE *f, const void *instance, void *env)
{
    const badResponse_t *r = (const badResponse_t *)instance;

    (void)env;
    DLFX_PBT_PrintWire(f, &r->wire);
    fprintf(f, "  (byte %u, was %02X)", r->index, r->was);
}

static void printBadInit(FILE *f, const void *instance, void *env)
{
    const badInit_t *b = (const badInit_t *)instance;

    (void)env;
    fprintf(f, "%s: ", deviationNames[b->deviation]);
    DLFX_PBT_PrintInit(f, &b->ecu);
}

static void printBadRequest(FILE *f, const void *instance, void *env)
{
    (void)env;
    fprintf(f, "dataLen %zu", ((const badRequest_t *)instance)->len);
}

/* Queues the response and runs ISO9141_RecvResponse */
static obd_status_t receive(const dlFxWire_t *w)
{
    obd_data_t resp = {0};
    size_t len = 0u;

    DLFX_PBT_QueueWire(w);
    return ISO9141_RecvResponse(&dlFx.link, &resp, &len);
}

static bool isOk(obd_status_t status)
{
    return (status.response == OBD_STATUS_OK) && (status.timeout == OBD_STATUS_OK);
}

static enum theft_trial_res propCorruptedRejected(struct theft *t, void *arg)
{
    (void)t;
    return isOk(receive(&((const badResponse_t *)arg)->wire)) ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS;
}

/* The frame is otherwise valid, so an early checksum match is no excuse here (that is F1) */
static enum theft_trial_res propWrongHeaderRejected(struct theft *t, void *arg)
{
    const dlFxWire_t *w = &((const badResponse_t *)arg)->wire;

    (void)t;
    if (DLFX_PBT_HasEarlyChecksum(w))
    {
        return THEFT_TRIAL_SKIP;
    }
    return isOk(receive(w)) ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS;
}

static enum theft_trial_res propTooLongRejected(struct theft *t, void *arg)
{
    const dlFxWire_t *w = &((const badResponse_t *)arg)->wire;

    (void)t;
    if (DLFX_PBT_HasEarlyChecksum(w))
    {
        return THEFT_TRIAL_SKIP;
    }
    return isOk(receive(w)) ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS;
}

static enum theft_trial_res propStalledRejected(struct theft *t, void *arg)
{
    const dlFxWire_t *w = &((const badResponse_t *)arg)->wire;
    obd_status_t status = {0};

    (void)t;
    if (DLFX_PBT_HasEarlyChecksum(w))
    {
        return THEFT_TRIAL_SKIP;
    }
    status = receive(w);
    return ((status.response == OBD_ERR_COMM_DATA_BYTE_NOT_RECVD) && (status.timeout == OBD_ERR_TIMEOUT_MAX))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

static enum theft_trial_res propEarlyRejected(struct theft *t, void *arg)
{
    obd_status_t status = {0};

    (void)t;
    status = receive(&((const badResponse_t *)arg)->wire);
    return ((status.response == OBD_ERR_COMM_FMT_BYTE_NOT_RECVD) && (status.timeout == OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

static enum theft_trial_res propRejectedReported(struct theft *t, void *arg)
{
    const dlFxWire_t *w = &((const badResponse_t *)arg)->wire;

    (void)t;
    if (DLFX_PBT_HasEarlyChecksum(w))
    {
        return THEFT_TRIAL_SKIP;
    }
    return (receive(w).response == OBD_ERR_COMM_ECU_RESPONSE_7F) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* The init fails with the error that names what went wrong, and leaves no protocol or header behind */
static enum theft_trial_res propBadInitRejected(struct theft *t, void *arg)
{
    const badInit_t *b = (const badInit_t *)arg;
    uint8_t protocol = 0xFFu;
    obd_status_t status = {0};

    (void)t;
    (void)DLFX_PBT_QueueInit(&b->ecu);
    status = ISO9141_Connect(&dlFx.link, &protocol);
    return ((status.response == deviationError[b->deviation]) && (protocol == 0xFFu) && (dlFx.proto.header.fmt == 0u))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Refused before anything happens: no bytes, no time */
static enum theft_trial_res propBadRequestRefused(struct theft *t, void *arg)
{
    const badRequest_t *r = (const badRequest_t *)arg;
    obd_status_t status = {0};

    (void)t;
    DLFX_SetUpLine();
    status = ISO9141_SendRequest(&dlFx.link, &r->data, r->len);
    return ((status.response == OBD_ERR_INVALID_PARAM) && (q_getCount(&dlFx.uart.eventQueue) == 0u)
         && (dlFx.timer.timeNow == 0u)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A corrupted response is never accepted
 * - description: For any valid response with one byte changed (no
 *                UART error, so only the checksum can tell), the
 *                response is rejected. Half the corruptions set the
 *                byte to the running sum before it, so they look like
 *                a checksum to a receiver that frames by checksum.
 * - steps:
 *     1. Generate a valid response, change one byte
 *     2. ISO9141_RecvResponse does not return OK
 ******************************************************************/
void test_neg_pbt_ISO9141_RecvResponse_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: a corrupted response is never accepted",
        .prop1     = propCorruptedRejected,
        .type_info = { &corruptedInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_KNOWN_BUG(&cfg, "test_neg_pbt_ISO9141_RecvResponse_0.vcd",
                  "F1: a byte changed to the running sum is taken as the checksum, a cut frame is OK (PROPERTY_TESTS.md)");
}

/******************************************************************
 * - brief:       A response with a foreign header is not accepted
 * - description: ISO 9141-2 responses to the tester start with 48 6B.
 *                For any otherwise valid frame whose format or target
 *                byte differs (checksum still right), RecvResponse
 *                must not return OK: it is not an answer to us.
 * - steps:
 *     1. Generate a valid response, change format or target, fix the checksum
 *     2. ISO9141_RecvResponse does not return OK
 ******************************************************************/
void test_neg_pbt_ISO9141_RecvResponse_1(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: a response with a foreign header is not accepted",
        .prop1     = propWrongHeaderRejected,
        .type_info = { &wrongHeaderInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_KNOWN_BUG(&cfg, "test_neg_pbt_ISO9141_RecvResponse_1.vcd",
                  "F3: the response header is never checked (PROPERTY_TESTS.md)");
}

/******************************************************************
 * - brief:       A response with more than 7 data bytes is rejected
 * - description: ISO 9141-2 carries at most 7 data bytes. For any
 *                frame of 8..12 data bytes with a correct checksum,
 *                RecvResponse must not return OK.
 * - steps:
 *     1. Generate a response with 8..12 data bytes
 *     2. ISO9141_RecvResponse does not return OK
 ******************************************************************/
void test_neg_pbt_ISO9141_RecvResponse_2(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: a response longer than 7 data bytes is rejected",
        .prop1     = propTooLongRejected,
        .type_info = { &tooLongInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_KNOWN_BUG(&cfg, "test_neg_pbt_ISO9141_RecvResponse_2.vcd",
                  "F4: frames up to 12 data bytes are accepted (PROPERTY_TESTS.md)");
}

/******************************************************************
 * - brief:       An ECU pause longer than P1 max ends the frame
 * - description: For any response where the ECU waits more than P1
 *                max before one byte, the tester gives up on the
 *                frame with OBD_ERR_COMM_DATA_BYTE_NOT_RECVD and a
 *                max timeout, instead of gluing the late bytes on.
 * - steps:
 *     1. Generate a response, delay one byte (and the rest) past P1 max
 *     2. RecvResponse: DATA_BYTE_NOT_RECVD + TIMEOUT_MAX
 ******************************************************************/
void test_neg_pbt_ISO9141_RecvResponse_3(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: a pause over P1 max ends the frame",
        .prop1     = propStalledRejected,
        .type_info = { &stalledInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_ISO9141_RecvResponse_3.vcd");
}

/******************************************************************
 * - brief:       A response before P2 min is rejected
 * - description: For any response whose format byte lands before
 *                P2 min, RecvResponse reports FMT_BYTE_NOT_RECVD with
 *                the P2 min timeout.
 * - steps:
 *     1. Generate a response, move it before P2 min
 *     2. RecvResponse: FMT_BYTE_NOT_RECVD + P2_TIMEOUT_MIN_TESTER_ECU
 ******************************************************************/
void test_neg_pbt_ISO9141_RecvResponse_4(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: a response before P2 min is rejected",
        .prop1     = propEarlyRejected,
        .type_info = { &earlyInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_ISO9141_RecvResponse_4.vcd");
}

/******************************************************************
 * - brief:       A negative response is always reported as such
 * - description: For any response with sid 7F (and any rest), the
 *                caller gets OBD_ERR_COMM_ECU_RESPONSE_7F.
 * - steps:
 *     1. Generate a 7F response of 1..7 data bytes
 *     2. RecvResponse returns OBD_ERR_COMM_ECU_RESPONSE_7F
 ******************************************************************/
void test_neg_pbt_ISO9141_RecvResponse_5(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: a negative response is reported",
        .prop1     = propRejectedReported,
        .type_info = { &rejectedInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_ISO9141_RecvResponse_5.vcd");
}

/******************************************************************
 * - brief:       A wrong or mistimed ECU answer fails the init
 * - description: For a correct 5-baud answer with exactly one thing
 *                wrong (a byte before or after its W window, a wrong
 *                sync, keyword bytes or inverted address),
 *                ISO9141_Connect fails with the error naming it,
 *                leaves the protocol untouched and stores no header.
 * - steps:
 *     1. Generate a correct answer and one deviation
 *     2. ISO9141_Connect returns the matching OBD_ERR_5BAUD_* error
 *     3. Protocol untouched, header empty
 ******************************************************************/
void test_neg_pbt_ISO9141_Connect_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: any wrong or mistimed init byte fails the init",
        .prop1     = propBadInitRejected,
        .type_info = { &badInitInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_ISO9141_Connect_0.vcd");
}

/******************************************************************
 * - brief:       A request length outside 1..7 is refused
 * - description: For data length 0 or 8..65535, send_request returns
 *                OBD_ERR_INVALID_PARAM before anything happens: no
 *                byte on the line, no time spent.
 * - steps:
 *     1. Generate an invalid length
 *     2. ISO9141_SendRequest: INVALID_PARAM, empty line, clock at 0
 ******************************************************************/
void test_neg_pbt_ISO9141_SendRequest_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: an invalid request length is refused",
        .prop1     = propBadRequestRefused,
        .type_info = { &badRequestInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}
