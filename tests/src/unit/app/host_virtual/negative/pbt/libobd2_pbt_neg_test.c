/* ================================================ INCLUDES =============================================== */
#include "libobd2_pbt_neg_test.h"
#include "datalink.h"
#include "libobd2_fixture.h"
#include "pbt_support.h"
#include "libobd2.h"
#include "libobd2_services.h"
#include "fault_injection.h"
#include "iso15031_5.h"
#include "statusRetCodes.h"
#include "theft.h"
#include "unity.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define RESPONSE_BUF_LEN 64u
#define INFO_SID         (SID_REQUEST_VEHICLE_INFO + 0x40u)
#define VIN_MSGS         5u   /* 17 VIN characters + 3 pad bytes, 4 per message */
#define MSG_LEN          7u   /* 49 <pid> <msgNr> D1..D4 */
#define CNT_LEN          3u   /* 49 <count pid> <count> */
#define NEGATIVE_SID     0x7Fu

/* One line byte of a VIN exchange gets an rx error: count answer 7 bytes + 5 x 11 */
#define VIN_EXCHANGE_BYTES (CNT_LEN + 4u + VIN_MSGS * (MSG_LEN + 4u))

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint16_t errCode;
} sendErrCase_t;

/* A VIN exchange as the ECU sends it: the count answer and the data messages, after the given changes */
typedef struct
{
    uint8_t cnt[CNT_LEN];
    uint8_t msg[VIN_MSGS][MSG_LEN];
    uint8_t changed; /* which answer was changed: 0 = count, 1.. = data message */
} vinExchange_t;

/* A malformed coolant answer: 'len' bytes of 'bytes' */
typedef struct
{
    uint8_t len;
    uint8_t bytes[3];
} coolantAnswer_t;

/* An rx error on one byte of a VIN exchange */
typedef struct
{
    vinExchange_t        exchange;
    uint8_t              byteIdx;
    obd_uart_rx_status_t error;
} brokenExchange_t;

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static enum theft_alloc_res alloc_sendErr(struct theft *t, void *env, void **output);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info sendErr_type_info = {
    .alloc = alloc_sendErr,
    .free  = theft_generic_free_cb,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */

/* --------------------------------------------------------------------------
 * Allocator: a non-OK transport error code.
 * -------------------------------------------------------------------------- */
static enum theft_alloc_res alloc_sendErr(struct theft *t, void *env, void **output)
{
    (void)env;
    sendErrCase_t *v = malloc(sizeof(sendErrCase_t));
    if (v == NULL) { return THEFT_ALLOC_ERROR; }

    /* OBD_STATUS_OK is 0; keep the generated code strictly non-zero. */
    v->errCode = (uint16_t)(1u + (theft_random_bits(t, 16) % 0xFFFFu));

    *output = v;
    return THEFT_ALLOC_OK;
}

/* --------------------------------------------------------------------------
 * Mock transport: a send that always fails with a caller-supplied code.
 * The paired receive stub must never be invoked — a failed send has to
 * short-circuit the transfer protocol before any response is read.
 * -------------------------------------------------------------------------- */
static obd_status_t mock_send_fail(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    (void)serviceRequests;
    (void)dataLen;
    uint16_t *errCode = (uint16_t *)self->pProtocolCtx;
    obd_status_t status = {0};
    status.response = *errCode;
    return status;
}

static obd_status_t mock_recv_should_not_be_called(dataLink_if_t *self, obd_data_t *resp, size_t *dataLen)
{
    (void)self;
    (void)resp;
    (void)dataLen;
    obd_status_t status = {0};
    status.response = OBD_GENERIC_ERROR;
    return status;
}

/* --------------------------------------------------------------------------
 * Property: when the transport fails to send the request, the failure must
 * propagate to the caller unchanged, and no response bytes/length may be
 * written — a diagnostic scanner must never report or fabricate partial
 * results after a transport error.
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_send_failure_propagates(struct theft *t, void *arg)
{
    (void)t;
    sendErrCase_t testCase = *(sendErrCase_t *)arg;
    uint16_t errCode = testCase.errCode;

    dataLink_if_t mockLink = APPFX_MockLink(&errCode, mock_send_fail, mock_recv_should_not_be_called);
    obd_ctx_t ctx = { .pDataLink = &mockLink };

    uint8_t response[RESPONSE_BUF_LEN] = {0};
    const size_t sentinel = (size_t)0xDEADBEEFu;
    size_t responseLen = sentinel;

    obd_status_t status = LibOBD2_RequestService(&ctx, OBD_GET_VIN, response, &responseLen);

    if (status.response != errCode)   { return THEFT_TRIAL_FAIL; }
    if (responseLen != sentinel)      { return THEFT_TRIAL_FAIL; }

    return THEFT_TRIAL_PASS;
}


/* ---- Line level: the whole stack on the virtual K-line ---- */
static const obd_uart_rx_status_t rxErrors[] =
{
    OBD_UART_RX_PARITY_ERR, OBD_UART_RX_FRAMING_ERR, OBD_UART_RX_OVERRUN_ERR, OBD_UART_RX_NOISE_ERR,
};

/* A correct VIN exchange with random data */
static void genVinExchange(struct theft *t, vinExchange_t *x)
{
    x->cnt[0] = INFO_SID;
    x->cnt[1] = PID_09_VIN_MESSAGE_COUNT;
    x->cnt[2] = VIN_MSGS;
    for (uint8_t idx = 0u; idx < VIN_MSGS; idx++)
    {
        x->msg[idx][0] = INFO_SID;
        x->msg[idx][1] = PID_09_VIN;
        x->msg[idx][2] = (uint8_t)(idx + 1u);
        for (uint8_t d = 3u; d < MSG_LEN; d++)
        {
            x->msg[idx][d] = PBT_Byte(t);
        }
    }
}

/* Message numbers that are not 1..5 in order: shuffled, repeated or out of range */
static enum theft_alloc_res allocMisnumbered(struct theft *t, void *env, void **output)
{
    vinExchange_t *x = calloc(1u, sizeof(*x));
    bool inOrder = true;

    (void)env;
    if (x == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    genVinExchange(t, x);
    for (uint8_t idx = 0u; idx < VIN_MSGS; idx++)
    {
        x->msg[idx][2] = PBT_OneIn(t, 2u) ? (uint8_t)PBT_Range(t, 0u, VIN_MSGS + 1u) : (uint8_t)(idx + 1u);
        inOrder = inOrder && (x->msg[idx][2] == (idx + 1u));
    }
    if (inOrder)
    {
        x->msg[0][2] = 2u;
        x->msg[1][2] = 1u;
    }
    *output = x;
    return THEFT_ALLOC_OK;
}

/* One answer (count or data) with a sid or PID that does not belong to the VIN request */
static enum theft_alloc_res allocMismatched(struct theft *t, void *env, void **output)
{
    vinExchange_t *x = calloc(1u, sizeof(*x));
    uint8_t *answer = NULL;
    uint8_t expected = 0u;
    uint8_t field = 0u;

    (void)env;
    if (x == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    genVinExchange(t, x);
    x->changed = (uint8_t)PBT_Range(t, 0u, VIN_MSGS);
    answer = (x->changed == 0u) ? x->cnt : x->msg[x->changed - 1u];
    field = PBT_OneIn(t, 2u) ? 1u : 0u; /* 0: sid, 1: PID */
    expected = answer[field];
    answer[field] = (uint8_t)(expected ^ (uint8_t)PBT_Range(t, 1u, 0xFFu));
    if ((field == 0u) && (answer[0] == NEGATIVE_SID))
    {
        answer[0] = SID_SHOW_CURRENT_DATA + 0x40u; /* a negative response is reported fine, keep it a wrong sid */
    }
    *output = x;
    return THEFT_ALLOC_OK;
}

/* 41 05 without A, or 41 alone */
static enum theft_alloc_res allocTruncatedCoolant(struct theft *t, void *env, void **output)
{
    coolantAnswer_t *c = calloc(1u, sizeof(*c));

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->bytes[0] = SID_SHOW_CURRENT_DATA + 0x40u;
    c->bytes[1] = PID_01_COOLANT_TEMP;
    c->len = (uint8_t)PBT_Range(t, 1u, 2u);
    *output = c;
    return THEFT_ALLOC_OK;
}

/* A complete answer, but for another PID or another service */
static enum theft_alloc_res allocOtherCoolant(struct theft *t, void *env, void **output)
{
    coolantAnswer_t *c = calloc(1u, sizeof(*c));

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->len = 3u;
    c->bytes[0] = SID_SHOW_CURRENT_DATA + 0x40u;
    c->bytes[1] = PID_01_COOLANT_TEMP;
    c->bytes[2] = PBT_Byte(t);
    if (PBT_OneIn(t, 2u))
    {
        c->bytes[1] = (uint8_t)(PID_01_COOLANT_TEMP ^ (uint8_t)PBT_Range(t, 1u, 0xFFu));
    }
    else
    {
        c->bytes[0] = (uint8_t)(c->bytes[0] ^ (uint8_t)PBT_Range(t, 1u, 0xFFu));
        c->bytes[0] = (c->bytes[0] == NEGATIVE_SID) ? 0x42u : c->bytes[0];
    }
    *output = c;
    return THEFT_ALLOC_OK;
}

static enum theft_alloc_res allocBrokenExchange(struct theft *t, void *env, void **output)
{
    brokenExchange_t *b = calloc(1u, sizeof(*b));

    (void)env;
    if (b == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    genVinExchange(t, &b->exchange);
    b->byteIdx = (uint8_t)PBT_Range(t, 0u, VIN_EXCHANGE_BYTES - 1u);
    b->error   = rxErrors[PBT_Range(t, 0u, (sizeof(rxErrors) / sizeof(rxErrors[0])) - 1u)];
    *output = b;
    return THEFT_ALLOC_OK;
}

static void printExchange(FILE *f, const vinExchange_t *x)
{
    fprintf(f, "%02X %02X %02X", x->cnt[0], x->cnt[1], x->cnt[2]);
    for (uint8_t idx = 0u; idx < VIN_MSGS; idx++)
    {
        fprintf(f, " |");
        for (uint8_t d = 0u; d < MSG_LEN; d++)
        {
            fprintf(f, " %02X", x->msg[idx][d]);
        }
    }
}

static void printVinExchange(FILE *f, const void *instance, void *env)
{
    (void)env;
    printExchange(f, (const vinExchange_t *)instance);
}

static void printCoolantAnswer(FILE *f, const void *instance, void *env)
{
    const coolantAnswer_t *c = (const coolantAnswer_t *)instance;

    (void)env;
    for (uint8_t idx = 0u; idx < c->len; idx++)
    {
        fprintf(f, "%s%02X", (idx != 0u) ? " " : "", c->bytes[idx]);
    }
}

static void printBrokenExchange(FILE *f, const void *instance, void *env)
{
    const brokenExchange_t *b = (const brokenExchange_t *)instance;

    (void)env;
    printExchange(f, &b->exchange);
    fprintf(f, "  (rx status %u on line byte %u)", (unsigned)b->error, b->byteIdx);
}

static struct theft_type_info misnumberedInfo    = PBT_TYPE_INFO(allocMisnumbered, printVinExchange);
static struct theft_type_info mismatchedInfo     = PBT_TYPE_INFO(allocMismatched, printVinExchange);
static struct theft_type_info truncatedInfo      = PBT_TYPE_INFO(allocTruncatedCoolant, printCoolantAnswer);
static struct theft_type_info otherCoolantInfo   = PBT_TYPE_INFO(allocOtherCoolant, printCoolantAnswer);
static struct theft_type_info brokenExchangeInfo = PBT_TYPE_INFO(allocBrokenExchange, printBrokenExchange);

/* Queues the exchange like APPFX_QueueVehicleInfo does: count answer after the first request, the data
 * messages back to back after the second */
static void queueVinExchange(const vinExchange_t *x)
{
    uint32_t time = 0u;

    time += APPFX_REQ_DURATION(2u);
    time  = APPFX_QueueEcuFrame(time, x->cnt, CNT_LEN);
    time += APPFX_REQ_DURATION(2u);
    for (uint8_t idx = 0u; idx < VIN_MSGS; idx++)
    {
        time = APPFX_QueueEcuFrame(time, x->msg[idx], MSG_LEN);
    }
}

static obd_status_t requestVin(uint8_t *response, size_t *responseLen)
{
    return LibOBD2_RequestService(&appFx.ctx, OBD_GET_VIN, response, responseLen);
}

/* Misnumbered: OK is only allowed with the data put back in message-number order */
static enum theft_trial_res propMisnumbered(struct theft *t, void *arg)
{
    const vinExchange_t *x = (const vinExchange_t *)arg;
    uint8_t ordered[VIN_MSGS * 4u] = {0};
    uint8_t seen = 0u;
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0u;
    obd_status_t status = {0};

    (void)t;
    for (uint8_t idx = 0u; idx < VIN_MSGS; idx++)
    {
        uint8_t nr = x->msg[idx][2];
        if ((nr >= 1u) && (nr <= VIN_MSGS) && (0u == (seen & (1u << nr))))
        {
            seen = (uint8_t)(seen | (1u << nr));
            memcpy(&ordered[(nr - 1u) * 4u], &x->msg[idx][3], 4u);
        }
    }
    APPFX_SetUpLine();
    queueVinExchange(x);
    status = requestVin(response, &responseLen);
    if ((status.response != OBD_STATUS_OK) || (status.timeout != OBD_STATUS_OK))
    {
        return THEFT_TRIAL_PASS;
    }
    /* accepted: only right when every number 1..5 came exactly once and the data was reordered */
    return ((seen == 0x3Eu) && (responseLen == sizeof(ordered)) && (0 == memcmp(response, ordered, sizeof(ordered))))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* An answer that does not belong to the request is never turned into an OK result */
static enum theft_trial_res propMismatched(struct theft *t, void *arg)
{
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0u;
    obd_status_t status = {0};

    (void)t;
    APPFX_SetUpLine();
    queueVinExchange((const vinExchange_t *)arg);
    status = requestVin(response, &responseLen);
    return (status.response == OBD_STATUS_OK) ? THEFT_TRIAL_FAIL : THEFT_TRIAL_PASS;
}

static enum theft_trial_res propCoolantRejected(struct theft *t, void *arg)
{
    const coolantAnswer_t *c = (const coolantAnswer_t *)arg;
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0u;
    obd_status_t status = {0};

    (void)t;
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), c->bytes, c->len);
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    return ((status.response != OBD_STATUS_OK) && (responseLen == 0u)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* A UART error anywhere in the exchange fails the request with no data */
static enum theft_trial_res propBrokenExchange(struct theft *t, void *arg)
{
    const brokenExchange_t *b = (const brokenExchange_t *)arg;
    faultUartInjector_t inj = { .onWrite = { .error = b->error, .firstByte = b->byteIdx, .count = 1u } };
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0u;
    obd_status_t status = {0};

    (void)t;
    APPFX_SetUpLine();
    if (!FAULT_UartAttach(&appFx.uart, &inj))
    {
        return THEFT_TRIAL_ERROR;
    }
    queueVinExchange(&b->exchange);
    status = requestVin(response, &responseLen);
    return ((status.response != OBD_STATUS_OK) && (responseLen == 0u) && (inj.injected == 1u))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A transport send failure is returned as is
 * - description: Property test (theft): when sending the request fails, the
 *                request stops there with the transport status untouched and
 *                the receive path is never called.
 * - steps:
 *     1. Generate a random failure status for the send stub
 *     2. RequestService returns exactly that status, recv never runs
 ******************************************************************/
void test_neg_pbt_LIBOBD2_RequestService_0(void)
{
    struct theft_run_config cfg = {
        .name      = "A transport send failure short-circuits with the status untouched",
        .prop1     = prop_send_failure_propagates,
        .type_info = { &sendErr_type_info },
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}

/******************************************************************
 * - brief:       VIN messages are assembled by message number
 * - description: Each $09 data message carries its number. For any
 *                VIN exchange whose numbers are not 1..5 in order
 *                (shuffled, repeated or out of range), the request
 *                must fail, or return the data reordered by number
 *                when every number 1..5 came exactly once.
 * - steps:
 *     1. Generate a VIN exchange, change some message numbers
 *     2. RequestService(VIN)
 *     3. Not OK, or OK with the data in message-number order
 ******************************************************************/
void test_neg_pbt_LIBOBD2_RequestService_1(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: VIN messages are assembled by message number",
        .prop1     = propMisnumbered,
        .type_info = { &misnumberedInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_KNOWN_BUG(&cfg, "test_neg_pbt_LIBOBD2_RequestService_1.vcd",
                  "F5 (#66): message numbers are ignored, data is joined in arrival order (PROPERTY_TESTS.md)");
}

/******************************************************************
 * - brief:       An answer for another request is never OK
 * - description: For a VIN exchange where one answer (the count or a
 *                data message) carries a sid other than 49 or a PID
 *                other than the one asked for, the request must not
 *                return OK: that data does not answer our request.
 * - steps:
 *     1. Generate a VIN exchange, change the sid or PID of one answer
 *     2. RequestService(VIN) does not return OK
 ******************************************************************/
void test_neg_pbt_LIBOBD2_RequestService_2(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: an answer for another request is never OK",
        .prop1     = propMismatched,
        .type_info = { &mismatchedInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_KNOWN_BUG(&cfg, "test_neg_pbt_LIBOBD2_RequestService_2.vcd",
                  "F6 (#67): sid and PID of $09 answers are not checked against the request (PROPERTY_TESTS.md)");
}

/******************************************************************
 * - brief:       A coolant answer without its data byte is rejected
 * - description: 41 05 needs one data byte A. An answer that stops
 *                before it (41 05, or 41 alone) must fail the request
 *                with no data, not decode a temperature from a byte
 *                that never came.
 * - steps:
 *     1. Generate a truncated answer
 *     2. RequestService(coolant): not OK, len 0
 ******************************************************************/
void test_neg_pbt_LIBOBD2_RequestService_3(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: a coolant answer without A is rejected",
        .prop1     = propCoolantRejected,
        .type_info = { &truncatedInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_KNOWN_BUG(&cfg, "test_neg_pbt_LIBOBD2_RequestService_3.vcd",
                  "F7 (#68): a missing data byte decodes as A = 0, i.e. -40 degC (PROPERTY_TESTS.md)");
}

/******************************************************************
 * - brief:       An answer for another PID or service is not coolant
 * - description: For a complete answer whose PID is not 05 or whose
 *                sid is not 41, RequestService(coolant) fails with no
 *                data instead of decoding a temperature.
 * - steps:
 *     1. Generate 41 xx A (xx != 05) or yy 05 A (yy != 41)
 *     2. RequestService(coolant): not OK, len 0
 ******************************************************************/
void test_neg_pbt_LIBOBD2_RequestService_4(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: an answer for another PID or service is not coolant",
        .prop1     = propCoolantRejected,
        .type_info = { &otherCoolantInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_LIBOBD2_RequestService_4.vcd");
}

/******************************************************************
 * - brief:       A UART error anywhere in an exchange fails it
 * - description: For a VIN exchange where any one ECU byte arrives
 *                with a parity, framing, overrun or noise error, the
 *                request fails and returns no data.
 * - steps:
 *     1. Generate a VIN exchange, a byte index and an rx error
 *     2. RequestService(VIN): not OK, len 0, the fault was applied
 ******************************************************************/
void test_neg_pbt_LIBOBD2_RequestService_5(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: a UART error anywhere fails the request",
        .prop1     = propBrokenExchange,
        .type_info = { &brokenExchangeInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_neg_pbt_LIBOBD2_RequestService_5.vcd");
}
