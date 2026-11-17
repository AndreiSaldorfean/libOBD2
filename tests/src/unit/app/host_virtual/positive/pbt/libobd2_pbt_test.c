/* ================================================ INCLUDES =============================================== */
#include "libobd2_pbt_test.h"
#include "datalink.h"
#include "libobd2_fixture.h"
#include "pbt_support.h"
#include "iso15031_5.h"
#include "libobd2.h"
#include "libobd2_services.h"
#include "statusRetCodes.h"
#include "theft.h"
#include "unity.h"
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define VIN_CHUNK_SIZE  4u    /* data bytes carried per SID $09 multi-frame message */
#define MAX_VIN_LEN     20u
#define RESPONSE_BUF_LEN 64u
#define MAX_INFO_MSGS    9u    /* $09 data messages per exchange: more would overflow the virtual line's
                                  128-byte queue (count answer + messages + the tester's echo) */
#define CANARY           0xA5u
#define CANARY_BUF_LEN   128u

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
typedef struct
{
    uint8_t len;
    uint8_t bytes[MAX_VIN_LEN];
} vinCase_t;

typedef struct
{
    uint8_t  vin[MAX_VIN_LEN];
    uint8_t  vinLen;
    uint8_t  msgCnt;
    uint8_t  callIdx;
} vinMockState_t;

static enum theft_alloc_res alloc_vin(struct theft *t, void *env, void **output);

/* ============================================ LOCAL VARIABLES ============================================ */
static struct theft_type_info vin_type_info = {
    .alloc = alloc_vin,
    .free  = theft_generic_free_cb,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */

/* --------------------------------------------------------------------------
 * Allocator: a VIN-like byte string of random (non-zero) length.
 * -------------------------------------------------------------------------- */
static enum theft_alloc_res alloc_vin(struct theft *t, void *env, void **output)
{
    (void)env;
    vinCase_t *v = malloc(sizeof(vinCase_t));
    if (v == NULL) { return THEFT_ALLOC_ERROR; }

    v->len = (uint8_t)(1u + (theft_random_bits(t, 8) % MAX_VIN_LEN));
    for (uint8_t idx = 0; idx < v->len; idx++)
    {
        v->bytes[idx] = (uint8_t)theft_random_bits(t, 8);
    }

    *output = v;
    return THEFT_ALLOC_OK;
}

/* --------------------------------------------------------------------------
 * Mock transport: always accepts the request.
 * -------------------------------------------------------------------------- */
static obd_status_t mock_send_ok(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen)
{
    (void)self;
    (void)serviceRequests;
    (void)dataLen;
    obd_status_t status = {0};
    return status;
}

/* --------------------------------------------------------------------------
 * Mock transport: replays a canned SID $09 (Request Vehicle Information)
 * exchange for PID $02 (VIN) — first the message-count response, then one
 * response per VIN_CHUNK_SIZE-byte chunk — driven from a vinMockState_t
 * stashed in self->pProtocolCtx.
 * -------------------------------------------------------------------------- */
static obd_status_t mock_recv_vin(dataLink_if_t *self, obd_data_t *resp, size_t *dataLen)
{
    vinMockState_t *state = (vinMockState_t *)self->pProtocolCtx;
    obd_status_t status = {0};

    memset(resp, 0, sizeof(*resp));
    resp->sid = (uint8_t)(SID_REQUEST_VEHICLE_INFO + 0x40u);

    if (state->callIdx == 0u)
    {
        resp->param[0] = PID_09_VIN_MESSAGE_COUNT;
        resp->param[1] = state->msgCnt;
        *dataLen = 2u;
    }
    else
    {
        uint8_t chunkIdx  = (uint8_t)(state->callIdx - 1u);
        uint8_t offset    = (uint8_t)(chunkIdx * VIN_CHUNK_SIZE);
        uint8_t remaining = (uint8_t)(state->vinLen - offset);
        uint8_t count     = (remaining < VIN_CHUNK_SIZE) ? remaining : (uint8_t)VIN_CHUNK_SIZE;

        resp->param[0] = PID_09_VIN;
        resp->param[1] = (uint8_t)(chunkIdx + 1u); /* message number */
        for (uint8_t idx = 0; idx < count; idx++)
        {
            resp->param[2u + idx] = state->vin[offset + idx];
        }
        *dataLen = 3u + count; /* sid, PID, message number, data */
    }

    state->callIdx++;
    return status;
}

/* --------------------------------------------------------------------------
 * Property: decoding a SID $09 VIN request must reassemble the exact bytes
 * the transport returned across every multi-frame message, in order,
 * regardless of how many chunks the VIN was split into.
 * -------------------------------------------------------------------------- */
static enum theft_trial_res prop_vin_roundtrip(struct theft *t, void *arg)
{
    (void)t;
    vinCase_t testCase = *(vinCase_t *)arg;

    vinMockState_t state = {0};
    memcpy(state.vin, testCase.bytes, testCase.len);
    state.vinLen  = testCase.len;
    state.msgCnt  = (uint8_t)((testCase.len + VIN_CHUNK_SIZE - 1u) / VIN_CHUNK_SIZE);
    state.callIdx = 0u;

    dataLink_if_t mockLink = APPFX_MockLink(&state, mock_send_ok, mock_recv_vin);
    obd_ctx_t ctx = { .pDataLink = &mockLink };

    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t  responseLen = 0;

    obd_status_t status = LibOBD2_RequestService(&ctx, OBD_GET_VIN, response, &responseLen);

    if (status.response != OBD_STATUS_OK)   { return THEFT_TRIAL_FAIL; }
    if (status.timeout   != OBD_STATUS_OK)   { return THEFT_TRIAL_FAIL; }
    if (responseLen != testCase.len)         { return THEFT_TRIAL_FAIL; }
    if (memcmp(response, testCase.bytes, testCase.len) != 0) { return THEFT_TRIAL_FAIL; }

    return THEFT_TRIAL_PASS;
}


/* ---- Line level: the whole stack on the virtual K-line ---- */
/* A $09 exchange: VIN or CALID, 'msgCnt' messages of 4 data bytes */
typedef struct
{
    bool    calid;
    uint8_t msgCnt;
    uint8_t payload[MAX_INFO_MSGS * VIN_CHUNK_SIZE];
} infoCase_t;

static enum theft_alloc_res allocInfo(struct theft *t, void *env, void **output)
{
    infoCase_t *c = calloc(1u, sizeof(*c));

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->calid  = PBT_OneIn(t, 2u);
    c->msgCnt = (uint8_t)PBT_Range(t, 1u, MAX_INFO_MSGS);
    for (size_t idx = 0u; idx < (size_t)c->msgCnt * VIN_CHUNK_SIZE; idx++)
    {
        c->payload[idx] = PBT_Byte(t);
    }
    *output = c;
    return THEFT_ALLOC_OK;
}

static enum theft_alloc_res allocCoolant(struct theft *t, void *env, void **output)
{
    uint8_t *a = malloc(sizeof(*a));

    (void)env;
    if (a == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    *a = PBT_Byte(t);
    *output = a;
    return THEFT_ALLOC_OK;
}

static void printInfo(FILE *f, const void *instance, void *env)
{
    const infoCase_t *c = (const infoCase_t *)instance;

    (void)env;
    fprintf(f, "%s, %u messages:", c->calid ? "CALID" : "VIN", c->msgCnt);
    for (size_t idx = 0u; idx < (size_t)c->msgCnt * VIN_CHUNK_SIZE; idx++)
    {
        fprintf(f, "%s%02X", ((idx % VIN_CHUNK_SIZE) == 0u) ? " | " : " ", c->payload[idx]);
    }
}

static void printCoolant(FILE *f, const void *instance, void *env)
{
    (void)env;
    fprintf(f, "A = 0x%02X", *(const uint8_t *)instance);
}

static struct theft_type_info infoInfo    = PBT_TYPE_INFO(allocInfo, printInfo);
static struct theft_type_info coolantInfo = PBT_TYPE_INFO(allocCoolant, printCoolant);

/* True when one of the case's ECU frames would be cut by F1 */
static bool infoHitsF1(const infoCase_t *c)
{
    const uint8_t sid = SID_REQUEST_VEHICLE_INFO + 0x40u;
    const uint8_t cnt[] = { sid, c->calid ? PID_09_CALID_MESSAGE_COUNT : PID_09_VIN_MESSAGE_COUNT, c->msgCnt };

    if (APPFX_HasEarlyChecksum(cnt, sizeof(cnt)))
    {
        return true;
    }
    for (uint8_t nr = 1u; nr <= c->msgCnt; nr++)
    {
        const uint8_t *d = &c->payload[(nr - 1u) * VIN_CHUNK_SIZE];
        const uint8_t msg[] = { sid, c->calid ? PID_09_CALID : PID_09_VIN, nr, d[0], d[1], d[2], d[3] };
        if (APPFX_HasEarlyChecksum(msg, sizeof(msg)))
        {
            return true;
        }
    }
    return false;
}

/* The data bytes of every message, in order, nothing written past them */
static enum theft_trial_res propInfoAnyData(struct theft *t, void *arg)
{
    const infoCase_t *c = (const infoCase_t *)arg;
    const size_t expectedLen = (size_t)c->msgCnt * VIN_CHUNK_SIZE;
    uint8_t response[CANARY_BUF_LEN];
    size_t responseLen = 0u;
    obd_status_t status = {0};

    (void)t;
    memset(response, CANARY, sizeof(response));
    APPFX_SetUpLine();
    APPFX_QueueVehicleInfo(c->calid ? PID_09_CALID_MESSAGE_COUNT : PID_09_VIN_MESSAGE_COUNT,
                           c->calid ? PID_09_CALID : PID_09_VIN, c->payload, c->msgCnt, c->msgCnt);
    status = LibOBD2_RequestService(&appFx.ctx, c->calid ? OBD_GET_CALID : OBD_GET_VIN, response, &responseLen);
    if ((status.response != OBD_STATUS_OK) || (status.timeout != OBD_STATUS_OK) || (responseLen != expectedLen)
     || (0 != memcmp(response, c->payload, expectedLen)))
    {
        return THEFT_TRIAL_FAIL;
    }
    for (size_t idx = expectedLen; idx < sizeof(response); idx++)
    {
        if (response[idx] != CANARY)
        {
            return THEFT_TRIAL_FAIL;
        }
    }
    return THEFT_TRIAL_PASS;
}

static enum theft_trial_res propInfoOverLine(struct theft *t, void *arg)
{
    return infoHitsF1((const infoCase_t *)arg) ? THEFT_TRIAL_SKIP : propInfoAnyData(t, arg);
}

/* Any A decodes to an int16_t, A - 40 */
static enum theft_trial_res propCoolantAnyA(struct theft *t, void *arg)
{
    const uint8_t a = *(const uint8_t *)arg;
    const uint8_t ecuResp[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_COOLANT_TEMP, a };
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0u;
    obd_status_t status = {0};
    int16_t temp = 0;

    (void)t;
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecuResp, sizeof(ecuResp));
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    memcpy(&temp, response, sizeof(temp));
    return ((status.response == OBD_STATUS_OK) && (responseLen == sizeof(temp)) && (temp == ((int16_t)a - 40)))
         ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

static enum theft_trial_res propCoolantOverLine(struct theft *t, void *arg)
{
    const uint8_t a = *(const uint8_t *)arg;
    const uint8_t data[] = { SID_SHOW_CURRENT_DATA + 0x40u, PID_01_COOLANT_TEMP, a };

    return APPFX_HasEarlyChecksum(data, sizeof(data)) ? THEFT_TRIAL_SKIP : propCoolantAnyA(t, arg);
}

/* Any data the ECU sends reaches the caller: VIN, CALID or coolant, no case skipped */
static enum theft_trial_res propAnyDataReachesCaller(struct theft *t, void *arg)
{
    const infoCase_t *c = (const infoCase_t *)arg;

    /* the coolant case reuses the first payload byte as A */
    return (c->msgCnt == 1u) && !c->calid ? propCoolantAnyA(t, (void *)&c->payload[0]) : propInfoAnyData(t, arg);
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A random VIN survives the SID $09 multi-frame exchange
 * - description: Property test (theft): LibOBD2_RequestService reassembles the
 *                exact VIN bytes the ECU split over several messages.
 * - steps:
 *     1. Generate a random VIN, script the ECU answers
 *     2. RequestService returns the same VIN (PBT_TRIALS trials)
 ******************************************************************/
void test_pos_pbt_LIBOBD2_RequestService_0(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2_RequestService reassembles a VIN across SID $09 multi-frame responses",
        .prop1     = prop_vin_roundtrip,
        .type_info = { &vin_type_info },
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}

/******************************************************************
 * - brief:       VIN and CALID come back complete over the K-line
 * - description: For VIN or CALID and any 1..9 messages of random
 *                data, the whole stack (request, count answer, data
 *                messages, decoding) returns OK with every data byte
 *                in message order, and writes nothing past them.
 *                Data that F1 cuts is skipped here (see _3).
 * - steps:
 *     1. Generate the service, the message count and the data
 *     2. Queue the ECU answers on the virtual line
 *     3. RequestService: OK, len = 4 x messages, same bytes
 *     4. The response buffer past len is untouched
 ******************************************************************/
void test_pos_pbt_LIBOBD2_RequestService_1(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: VIN/CALID come back complete over the K-line",
        .prop1     = propInfoOverLine,
        .type_info = { &infoInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_LIBOBD2_RequestService_1.vcd");
}

/******************************************************************
 * - brief:       Any coolant byte decodes to A - 40
 * - description: For any A in 41 05 A, RequestService(coolant)
 *                returns OK with an int16_t, A - 40.
 *                The one A that F1 cuts (0x0B) is skipped here (see _3).
 * - steps:
 *     1. Generate A, queue 48 6B 12 41 05 A CS
 *     2. RequestService: OK, len 2, (int16)(A - 40)
 ******************************************************************/
void test_pos_pbt_LIBOBD2_RequestService_2(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: any coolant byte decodes to A - 40",
        .prop1     = propCoolantOverLine,
        .type_info = { &coolantInfo },
        .trials    = PBT_TRIALS_LINE,
    };
    PBT_ASSERT_HOLDS(&cfg, "test_pos_pbt_LIBOBD2_RequestService_2.vcd");
}

/******************************************************************
 * - brief:       Any data the ECU sends reaches the caller
 * - description: Same exchanges as _1 and _2 (VIN, CALID, coolant),
 *                with nothing skipped: whatever bytes the vehicle
 *                holds, the caller gets them. Shows what F1 means for
 *                a user: some VINs, calibration IDs and one coolant
 *                value cannot be read at all.
 * - steps:
 *     1. Generate VIN/CALID data, or a coolant byte (1-message VIN case)
 *     2. RequestService returns OK with exactly that data
 ******************************************************************/
void test_pos_pbt_LIBOBD2_RequestService_3(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: any data the ECU sends reaches the caller",
        .prop1     = propAnyDataReachesCaller,
        .type_info = { &infoInfo },
    };
    PBT_KNOWN_BUG(&cfg, "test_pos_pbt_LIBOBD2_RequestService_3.vcd",
                  "F1 at app level (#62): VINs/CALIDs/coolant values with a byte equal to the running sum cannot be read");
}
