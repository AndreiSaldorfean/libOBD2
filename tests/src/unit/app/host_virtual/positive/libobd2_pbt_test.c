/* ================================================ INCLUDES =============================================== */
#include "libobd2_pbt_test.h"
#include "datalink.h"
#include "iso15031_5.h"
#include "libobd2.h"
#include "libobd2_services.h"
#include "statusRetCodes.h"
#include "theft.h"
#include "unity.h"
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define PBT_TRIALS      500u
#define VIN_CHUNK_SIZE  4u    /* data bytes carried per SID $09 multi-frame message */
#define MAX_VIN_LEN     20u
#define RESPONSE_BUF_LEN 64u

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
        for (uint8_t idx = 0; idx < count; idx++)
        {
            resp->param[3u + idx] = state->vin[offset + idx];
        }
        *dataLen = 4u + count;
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

    dataLink_if_t mockLink =
    {
        .pProtocolCtx  = &state,
        .send_request  = mock_send_ok,
        .recv_response = mock_recv_vin,
    };
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

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A random VIN survives the SID $09 multi-frame exchange
 * - description: Property test (theft): LibOBD2_RequestService reassembles the
 *                exact VIN bytes the ECU split over several messages.
 * - steps:
 *     1. Generate a random VIN, script the ECU answers
 *     2. RequestService returns the same VIN (PBT_TRIALS trials)
 ******************************************************************/
void test_pos_LIBOBD2_PBT_RequestService_0(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2_RequestService reassembles a VIN across SID $09 multi-frame responses",
        .prop1     = prop_vin_roundtrip,
        .type_info = { &vin_type_info },
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, theft_run(&cfg));
}
