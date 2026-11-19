/* ================================================ INCLUDES =============================================== */
#include "libobd2_services_pbt_neg_test.h"
#include "pbt_support.h"
#include "iso15031_5.h"
#include "libobd2.h"
#include "libobd2_formulas.h"
#include "libobd2_services.h"
#include "statusRetCodes.h"
#include "theft.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define RESPONSE_BUF_LEN 64u
#define NO_LEN           ((size_t)0xDEADu)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* A $01 answer for one entry of the PID formula table, cut short */
typedef struct
{
    uint8_t formulaIdx;
    uint8_t keptData;   /* data bytes left, fewer than the PID needs */
    uint8_t data[4];
} cutAnswer_t;

/* ============================================ LOCAL VARIABLES ============================================ */
extern const libobd2_pid_formula_t libobd2PidFormulas[];
extern const size_t libobd2PidFormulaCnt;
extern obd_status_t LibOBD2_DecodeResponse(const libobd2_data_t *request, const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen);

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static enum theft_alloc_res allocCutAnswer(struct theft *t, void *env, void **output)
{
    cutAnswer_t *c = calloc(1u, sizeof(*c));

    (void)env;
    if (c == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    c->formulaIdx = (uint8_t)PBT_Range(t, 0u, (uint32_t)libobd2PidFormulaCnt - 1u);
    c->keptData   = (uint8_t)PBT_Range(t, 0u, libobd2PidFormulas[c->formulaIdx].dataLen - 1u);
    for (size_t idx = 0; idx < sizeof(c->data); idx++)
    {
        c->data[idx] = PBT_Byte(t);
    }
    *output = c;
    return THEFT_ALLOC_OK;
}

static void printCutAnswer(FILE *f, const void *instance, void *env)
{
    const cutAnswer_t *c = (const cutAnswer_t *)instance;

    (void)env;
    fprintf(f, "PID %02X, %u of %u data bytes", libobd2PidFormulas[c->formulaIdx].pid, c->keptData,
            libobd2PidFormulas[c->formulaIdx].dataLen);
}

static struct theft_type_info cutAnswerInfo = PBT_TYPE_INFO(allocCutAnswer, printCutAnswer);

/* An answer with fewer data bytes than its PID needs fails with OBD_ERR_APP_SHORT_MSG and no data */
static enum theft_trial_res propCutAnswerRejected(struct theft *t, void *arg)
{
    const cutAnswer_t *c = (const cutAnswer_t *)arg;
    const uint8_t pid = libobd2PidFormulas[c->formulaIdx].pid;
    const libobd2_data_t request = { .data = { .sid = SID_SHOW_CURRENT_DATA, .param = { pid } }, .dataLen = 2u };
    libobd2_data_t answer = { .data = { .sid = SID_SHOW_CURRENT_DATA + 0x40u, .param = { pid } }, .dataLen = 2u + c->keptData };
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = NO_LEN;
    obd_status_t status = {0};

    (void)t;
    /* the bytes past the cut are still in the buffer: the decoder must not read them */
    memcpy(&answer.data.param[1], c->data, sizeof(c->data));
    status = LibOBD2_DecodeResponse(&request, &answer, 1u, response, &responseLen);
    return ((OBD_ERR_APP_SHORT_MSG == status.response) && (0u == responseLen)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A $01 answer cut before its last data byte fails
 * - description: Property test (theft): for every PID of the formula
 *                table and any cut (0 .. needed - 1 data bytes kept),
 *                decoding fails with OBD_ERR_APP_SHORT_MSG and no
 *                data, even with the missing bytes left in the buffer.
 *                F7 for every PID.
 * - steps:
 *     1. Generate a PID, a cut and the data bytes
 *     2. DecodeResponse: OBD_ERR_APP_SHORT_MSG, len 0
 ******************************************************************/
void test_neg_pbt_LIBOBD2_DecodeResponse_0(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: a $01 answer cut before its last data byte fails",
        .prop1     = propCutAnswerRejected,
        .type_info = { &cutAnswerInfo },
        .trials    = PBT_TRIALS_FAST,
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}
