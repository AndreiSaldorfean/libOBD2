/* ================================================ INCLUDES =============================================== */
#include "libobd2_pbt_neg_test.h"
#include "datalink.h"
#include "libobd2.h"
#include "libobd2_services.h"
#include "statusRetCodes.h"
#include "theft.h"
#include "unity.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define PBT_TRIALS       500u
#define RESPONSE_BUF_LEN 64u

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint16_t errCode;
} sendErrCase_t;

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

    dataLink_if_t mockLink =
    {
        .pProtocolCtx  = &errCode,
        .send_request  = mock_send_fail,
        .recv_response = mock_recv_should_not_be_called,
    };
    obd_ctx_t ctx = { .pDataLink = &mockLink };

    uint8_t response[RESPONSE_BUF_LEN] = {0};
    const size_t sentinel = (size_t)0xDEADBEEFu;
    size_t responseLen = sentinel;

    obd_status_t status = LibOBD2_RequestService(&ctx, OBD_GET_VIN, response, &responseLen);

    if (status.response != errCode)   { return THEFT_TRIAL_FAIL; }
    if (responseLen != sentinel)      { return THEFT_TRIAL_FAIL; }

    return THEFT_TRIAL_PASS;
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
        .trials    = PBT_TRIALS,
    };
    TEST_ASSERT_EQUAL(THEFT_RUN_PASS, theft_run(&cfg));
}
