/* ================================================ INCLUDES =============================================== */
#include "iso9141_pbt_test.h"
#include "dl_fixture.h"
#include "dl_pbt_support.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "datalink_test_vectors_000.h"
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
#include <string.h>

/* ================================================= MACROS ================================================ */
#define MSG_DATA_LEN  7u  /* sid + 6 params */
#define MSG_WIRE_LEN  11u /* 3 header + 7 data + 1 checksum */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* A request the tester sends: 1..7 data bytes */
typedef struct
{
    uint8_t    len;
    obd_data_t data;
} requestCase_t;


/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t ISO9141_ComputeChecksum(const header_t *header, const obd_data_t *data, size_t dataLen);
extern size_t ISO9141_SerializeMessage(const message_t *msg, uint8_t *frame, size_t dataLen);
extern obd_status_t ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t *len);

static enum theft_alloc_res allocMessage(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocRequest(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocResponse(struct theft *t, void *env, void **output);
static enum theft_alloc_res allocInit(struct theft *t, void *env, void **output);
static void printMessage(FILE *f, const void *instance, void *env);
static void printRequest(FILE *f, const void *instance, void *env);
static void printWire(FILE *f, const void *instance, void *env);
static void printInit(FILE *f, const void *instance, void *env);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info messageInfo  = PBT_TYPE_INFO(allocMessage, printMessage);
static struct theft_type_info requestInfo  = PBT_TYPE_INFO(allocRequest, printRequest);
static struct theft_type_info responseInfo = PBT_TYPE_INFO(allocResponse, printWire);
static struct theft_type_info initInfo     = PBT_TYPE_INFO(allocInit, printInit);

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static enum theft_alloc_res allocMessage(struct theft *t, void *env, void **output)
{
    message_t *m = calloc(1u, sizeof(*m));
    uint8_t *raw = (uint8_t *)m;

    (void)env;
    if (m == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    for (size_t idx = 0u; idx < sizeof(*m); idx++)
    {
        raw[idx] = PBT_Byte(t);
    }
    *output = m;
    return THEFT_ALLOC_OK;
}

static enum theft_alloc_res allocRequest(struct theft *t, void *env, void **output)
{
    requestCase_t *c = calloc(1u, sizeof(*c));
    uint8_t *raw = NULL;

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->len = (uint8_t)PBT_Range(t, 1u, MSG_DATA_LEN);
    raw = (uint8_t *)&c->data;
    for (uint8_t idx = 0u; idx < c->len; idx++)
    {
        raw[idx] = PBT_Byte(t);
    }
    *output = c;
    return THEFT_ALLOC_OK;
}

static enum theft_alloc_res allocResponse(struct theft *t, void *env, void **output)
{
    dlFxWire_t *w = calloc(1u, sizeof(*w));

    (void)env;
    if (w == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    DLFX_PBT_GenResponse(t, w, (uint8_t)PBT_Range(t, 1u, DLFX_PBT_MAX_DATA), -1);
    *output = w;
    return THEFT_ALLOC_OK;
}

/* A correct ECU answer: keyword bytes 08 08 or 94 94, W1..W3 up to one poll short of their max, W4
 * two short (the tester checks its timeout right after the poll that misses a byte due at the deadline) */
static enum theft_alloc_res allocInit(struct theft *t, void *env, void **output)
{
    dlFxInit_t *c = calloc(1u, sizeof(*c));

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->sync    = 0x55u;
    c->kb1     = PBT_OneIn(t, 2u) ? 0x94u : 0x08u;
    c->kb2     = c->kb1;
    c->invAddr = (uint8_t)~DLFX_TARGET_ADDR;
    c->w1  = PBT_Range(t, ISO9141_W1_TIME_MIN, ISO9141_W1_TIME_MAX - 1u);
    c->w2  = PBT_Range(t, ISO9141_W2_TIME_MIN, ISO9141_W2_TIME_MAX - 1u);
    c->w3  = PBT_Range(t, ISO9141_W3_TIME_MIN, ISO9141_W3_TIME_MAX - 1u);
    c->w4  = PBT_Range(t, ISO9141_W4_TIME_MIN, ISO9141_W4_TIME_MAX - 2u);
    *output = c;
    return THEFT_ALLOC_OK;
}

static void printMessage(FILE *f, const void *instance, void *env)
{
    const message_t *m = (const message_t *)instance;

    (void)env;
    fprintf(f, "%02X %02X %02X | %02X", m->header.fmt, m->header.trgAddr, m->header.srcAddr, m->data.sid);
    for (uint8_t idx = 0u; idx < (MSG_DATA_LEN - 1u); idx++)
    {
        fprintf(f, " %02X", m->data.param[idx]);
    }
    fprintf(f, " | cs %02X", m->cs);
}

static void printRequest(FILE *f, const void *instance, void *env)
{
    const requestCase_t *c = (const requestCase_t *)instance;
    const uint8_t *raw = (const uint8_t *)&c->data;

    (void)env;
    fprintf(f, "len %u:", c->len);
    for (uint8_t idx = 0u; idx < c->len; idx++)
    {
        fprintf(f, " %02X", raw[idx]);
    }
}

static void printWire(FILE *f, const void *instance, void *env)
{
    (void)env;
    DLFX_PBT_PrintWire(f, (const dlFxWire_t *)instance);
}

static void printInit(FILE *f, const void *instance, void *env)
{
    (void)env;
    DLFX_PBT_PrintInit(f, (const dlFxInit_t *)instance);
}

/* The checksum is the byte sum of header and data */
static enum theft_trial_res propChecksum(struct theft *t, void *arg)
{
    const message_t *m = (const message_t *)arg;
    uint8_t frame[MSG_WIRE_LEN] = {0};

    (void)t;
    (void)ISO9141_SerializeMessage(m, frame, MSG_DATA_LEN);
    return (ISO9141_ComputeChecksum(&m->header, &m->data, MSG_DATA_LEN) == DLFX_PBT_Checksum(frame, MSG_WIRE_LEN - 1u))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Serialization follows the wire layout [fmt trg src sid param.. cs] */
static enum theft_trial_res propWireLayout(struct theft *t, void *arg)
{
    const message_t *m = (const message_t *)arg;
    uint8_t frame[MSG_WIRE_LEN] = {0};
    const uint8_t expected[MSG_WIRE_LEN] =
    {
        m->header.fmt, m->header.trgAddr, m->header.srcAddr, m->data.sid,
        m->data.param[0], m->data.param[1], m->data.param[2], m->data.param[3], m->data.param[4],
        m->data.param[5], m->cs,
    };

    (void)t;
    if (MSG_WIRE_LEN != ISO9141_SerializeMessage(m, frame, MSG_DATA_LEN))
    {
        return THEFT_TRIAL_FAIL;
    }
    return (0 == memcmp(frame, expected, MSG_WIRE_LEN)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* A frame without an early checksum match reads back unchanged through ISO9141_RecvMessage */
static enum theft_trial_res propRecvMessageRoundtrip(struct theft *t, void *arg)
{
    message_t msg = *(const message_t *)arg;
    dlFxWire_t w = {0};
    message_t recvd = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    (void)t;
    msg.header.len = 0u; /* no length byte on the wire */
    memset(&msg.data.param[MSG_DATA_LEN - 1u], 0, sizeof(msg.data.param) - (MSG_DATA_LEN - 1u));
    msg.cs = ISO9141_ComputeChecksum(&msg.header, &msg.data, MSG_DATA_LEN);
    w.len = (uint8_t)ISO9141_SerializeMessage(&msg, w.byte, MSG_DATA_LEN);
    for (uint8_t idx = 0u; idx < w.len; idx++)
    {
        w.at[idx] = P2_TIME_MIN + idx;
    }
    if (DLFX_PBT_HasEarlyChecksum(&w))
    {
        return THEFT_TRIAL_SKIP; /* covered (and failing) in test_pos_pbt_ISO9141_RecvResponse_0 */
    }
    DLFX_PBT_QueueWire(&w);
    status = ISO9141_RecvMessage(&dlFx.link, &recvd, &len);
    if ((status.response != OBD_STATUS_OK) || (status.timeout != OBD_STATUS_OK) || (len != MSG_DATA_LEN))
    {
        return THEFT_TRIAL_FAIL;
    }
    return (0 == memcmp(&recvd, &msg, sizeof(msg))) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* A request goes out as header + data + checksum, after P3, with P4 between bytes, and P2 runs from
 * the end of it */
static enum theft_trial_res propSendRequestOnWire(struct theft *t, void *arg)
{
    const requestCase_t *c = (const requestCase_t *)arg;
    const uint8_t *raw = (const uint8_t *)&c->data;
    uint8_t expected[DLFX_PBT_WIRE_MAX] = { 0x68u, 0x6Au, 0xF1u };
    uint8_t expectedLen = DLFX_PBT_HEADER_LEN;
    uint32_t prevAt = 0u;
    uartEvent_t ev = {0};
    obd_status_t status = {0};

    (void)t;
    memcpy(&expected[expectedLen], raw, c->len);
    expectedLen += c->len;
    expected[expectedLen] = DLFX_PBT_Checksum(expected, expectedLen);
    expectedLen++;

    DLFX_SetUpLine();
    status = ISO9141_SendRequest(&dlFx.link, &c->data, c->len);
    if ((status.response != OBD_STATUS_OK) || (q_getCount(&dlFx.uart.eventQueue) != expectedLen))
    {
        return THEFT_TRIAL_FAIL;
    }
    for (uint8_t idx = 0u; idx < expectedLen; idx++)
    {
        (void)q_pop(&dlFx.uart.eventQueue, &ev);
        if ((ev.byte != expected[idx]) || (ev.timeReceived != (idx == 0u ? P3_TIME_MIN : prevAt + P4_TIME_MIN)))
        {
            return THEFT_TRIAL_FAIL;
        }
        prevAt = ev.timeReceived;
    }
    /* P2 starts once the last byte (and its P4) is out */
    return ((dlFx.link.timeSample == (prevAt + P4_TIME_MIN)) && dlFx.timer.timeoutStarted)
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Any valid ECU response inside the P1/P2 timing is returned exactly as sent */
static enum theft_trial_res propRecvResponseRoundtrip(struct theft *t, void *arg)
{
    const dlFxWire_t *w = (const dlFxWire_t *)arg;
    const uint8_t dataLen = (uint8_t)(w->len - DLFX_PBT_HEADER_LEN - 1u);
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    (void)t;
    DLFX_PBT_QueueWire(w);
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    if ((status.response != OBD_STATUS_OK) || (status.timeout != OBD_STATUS_OK) || (len != dataLen))
    {
        return THEFT_TRIAL_FAIL;
    }
    return (0 == memcmp(&resp, &w->byte[DLFX_PBT_HEADER_LEN], dataLen)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* An ECU answering inside every W window connects, with the header that goes with its keyword bytes */
static enum theft_trial_res propConnectInWindows(struct theft *t, void *arg)
{
    const dlFxInit_t *c = (const dlFxInit_t *)arg;
    const bool kwp = (c->kb1 == 0x94u);
    const header_t expected = kwp ? (header_t){ 0xC0u, 0x33u, 0xF1u, 0u } : (header_t){ 0x68u, 0x6Au, 0xF1u, 0u };
    uint32_t sendAt = 0u;
    uint8_t protocol = 0xFFu;
    obd_status_t status = {0};

    (void)t;
    sendAt = DLFX_PBT_QueueInit(c);
    status = ISO9141_Connect(&dlFx.link, &protocol);
    if ((status.response != OBD_STATUS_OK) || (status.timeout != OBD_STATUS_OK) || (protocol != ISO9141))
    {
        return THEFT_TRIAL_FAIL;
    }
    if ((dlFx.proto.header.fmt != expected.fmt) || (dlFx.proto.header.trgAddr != expected.trgAddr)
     || (dlFx.proto.header.srcAddr != expected.srcAddr) || (dlFx.proto.kb1 != c->kb1) || (dlFx.proto.kb2 != c->kb2))
    {
        return THEFT_TRIAL_FAIL;
    }
    /* the ECU timing was built on the tester model: make sure ~KB2 really went out then */
    return DLFX_PBT_SentAt((uint8_t)~c->kb2, sendAt) ? THEFT_TRIAL_PASS : THEFT_TRIAL_ERROR;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       The checksum is the byte sum of header and data
 * - description: For any message, ISO9141_ComputeChecksum equals the
 *                sum (mod 256) of every serialized byte before the
 *                checksum.
 * - steps:
 *     1. Generate a message, serialize it
 *     2. ComputeChecksum == sum of the first 10 wire bytes
 ******************************************************************/
void test_pos_pbt_ISO9141_ComputeChecksum_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: checksum is the byte sum",
        .prop1     = propChecksum,
        .type_info = { &messageInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}

/******************************************************************
 * - brief:       Serialized frames follow the wire layout
 * - description: For any message, ISO9141_SerializeMessage writes
 *                fmt, target, source, sid, the params and the
 *                checksum, in that order, 11 bytes for 7 data bytes.
 * - steps:
 *     1. Generate a message
 *     2. Serialize it: 11 bytes in wire order
 ******************************************************************/
void test_pos_pbt_ISO9141_SerializeMessage_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: serialization follows the wire layout",
        .prop1     = propWireLayout,
        .type_info = { &messageInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}

/******************************************************************
 * - brief:       An unambiguous frame reads back unchanged
 * - description: For any message whose bytes never match the running
 *                checksum early, ISO9141_RecvMessage returns header,
 *                data and checksum as sent. Frames with an early match
 *                are skipped here; test_pos_pbt_ISO9141_RecvResponse_0
 *                covers them.
 * - steps:
 *     1. Generate a message, put it on the line 1 ms apart after P2 min
 *     2. ISO9141_RecvMessage returns OK, 7 data bytes, the same message
 ******************************************************************/
void test_pos_pbt_ISO9141_RecvMessage_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: an unambiguous frame reads back unchanged",
        .prop1     = propRecvMessageRoundtrip,
        .type_info = { &messageInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_ISO9141_RecvMessage_0.vcd");
}

/******************************************************************
 * - brief:       A request goes out as header, data and checksum
 * - description: For any request of 1..7 data bytes, the line holds
 *                68 6A F1, the data and the checksum, the first byte
 *                after P3 min, the others P4 min apart, and the P2
 *                window starts after the last byte.
 * - steps:
 *     1. Generate 1..7 data bytes
 *     2. ISO9141_SendRequest returns OK
 *     3. Line bytes and their times match the wire layout and P3/P4
 *     4. P2 is armed, sampled at the end of the request
 ******************************************************************/
void test_pos_pbt_ISO9141_SendRequest_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: a request goes out with the right bytes and timing",
        .prop1     = propSendRequestOnWire,
        .type_info = { &requestInfo },
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_ISO9141_SendRequest_0.vcd");
}

/******************************************************************
 * - brief:       Any valid ECU response is returned as sent
 * - description: For any response 48 6B <src> with 1..7 data bytes
 *                and a correct checksum, first byte inside P2 and the
 *                rest inside P1, RecvResponse returns OK with exactly
 *                the data bytes and their count. ISO 9141-2 frames end
 *                after the checksum, so a data byte that happens to
 *                equal the running sum must not end the frame early.
 * - steps:
 *     1. Generate the response and its timing, queue it
 *     2. ISO9141_RecvResponse returns OK
 *     3. Same data, same length
 ******************************************************************/
void test_pos_pbt_ISO9141_RecvResponse_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: any valid response is returned as sent",
        .prop1     = propRecvResponseRoundtrip,
        .type_info = { &responseInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_KNOWN_BUG(&cfg, "test_pos_pbt_ISO9141_RecvResponse_0.vcd",
                  "F1: a data byte equal to the running checksum ends the frame early (PROPERTY_TESTS.md)");
}

/******************************************************************
 * - brief:       An ECU inside every W window connects
 * - description: For keyword bytes 08 08 or 94 94 and any W1..W4
 *                inside their windows (W4 counted from the tester's
 *                ~KB2), ISO9141_Connect returns OK with the header that
 *                belongs to the keyword bytes.
 * - steps:
 *     1. Generate the keyword bytes and W1..W4
 *     2. Queue 55 KB1 KB2 and CC (W4 after the tester's ~KB2)
 *     3. ISO9141_Connect returns OK, protocol ISO9141
 *     4. Header 68 6A F1 (08 08) or C0 33 F1 (94 94), kb stored
 ******************************************************************/
void test_pos_pbt_ISO9141_Connect_0(void)
{
    struct theft_run_config cfg = {
        .name      = "ISO9141: an ECU inside every W window connects",
        .prop1     = propConnectInWindows,
        .type_info = { &initInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_ISO9141_Connect_0.vcd");
}
