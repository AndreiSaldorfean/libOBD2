/* ================================================ INCLUDES =============================================== */
#include "libobd2_services_pbt_test.h"
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
#define RANGE_SLACK      0.001  /* float rounding at the ends of a range */
#define MAX_DTC_MSGS     6u     /* answers of one DTC exchange in a case */
#define DTC_SLOTS        3u     /* DTCs per answer */
#define RESPONSE_BUF_LEN 64u

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef enum
{
    VAL_U8,
    VAL_U16,
    VAL_U32,
    VAL_I16,
    VAL_F32,
} valType_t;

/* A $01 service, the type it returns and the Annex B min / max of its value */
typedef struct
{
    libobd2_sid_t service;
    valType_t type;
    double min;
    double max;
} rangeCase_t;

/* A generated $01 answer: which rangeCases[] entry, and the data bytes A..D */
typedef struct
{
    uint8_t caseIdx;
    uint8_t data[4];
} pidAnswer_t;

/* A generated $03 exchange: msgCnt answers of 3 DTC slots (0 = fill) */
typedef struct
{
    uint8_t msgCnt;
    uint16_t dtcs[MAX_DTC_MSGS][DTC_SLOTS];
} dtcExchange_t;

/* ============================================ LOCAL VARIABLES ============================================ */
extern const libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES];
extern const libobd2_pid_formula_t libobd2PidFormulas[];
extern obd_status_t LibOBD2_DecodeResponse(const libobd2_data_t *request, const libobd2_data_t *buffer, size_t bufferLen, uint8_t *response, size_t* responseLen);

static const rangeCase_t rangeCases[] =
{
    { OBD_GET_COOLANT_TEMP,             VAL_I16, -40.0, 215.0 },
    { OBD_GET_PIDS_SUPPORTED_01_20,     VAL_U32, 0.0, 4294967295.0 },
    { OBD_GET_PIDS_SUPPORTED_21_40,     VAL_U32, 0.0, 4294967295.0 },
    { OBD_GET_PIDS_SUPPORTED_41_60,     VAL_U32, 0.0, 4294967295.0 },
    { OBD_GET_FREEZE_DTC,               VAL_U16, 0.0, 65535.0 },
    { OBD_GET_ENGINE_LOAD,              VAL_F32, 0.0, 100.0 },
    { OBD_GET_SHORT_FUEL_TRIM_BANK1,    VAL_F32, -100.0, 99.22 },
    { OBD_GET_LONG_FUEL_TRIM_BANK1,     VAL_F32, -100.0, 99.22 },
    { OBD_GET_SHORT_FUEL_TRIM_BANK2,    VAL_F32, -100.0, 99.22 },
    { OBD_GET_LONG_FUEL_TRIM_BANK2,     VAL_F32, -100.0, 99.22 },
    { OBD_GET_FUEL_PRESSURE,            VAL_U16, 0.0, 765.0 },
    { OBD_GET_INTAKE_MAP,               VAL_U8,  0.0, 255.0 },
    { OBD_GET_ENGINE_RPM,               VAL_F32, 0.0, 16383.75 },
    { OBD_GET_VEHICLE_SPEED,            VAL_U8,  0.0, 255.0 },
    { OBD_GET_TIMING_ADVANCE,           VAL_F32, -64.0, 63.5 },
    { OBD_GET_INTAKE_AIR_TEMP,          VAL_I16, -40.0, 215.0 },
    { OBD_GET_MAF_FLOW_RATE,            VAL_F32, 0.0, 655.35 },
    { OBD_GET_THROTTLE_POSITION,        VAL_F32, 0.0, 100.0 },
    { OBD_GET_RUN_TIME,                 VAL_U16, 0.0, 65535.0 },
    { OBD_GET_DISTANCE_WITH_MIL,        VAL_U16, 0.0, 65535.0 },
    { OBD_GET_FUEL_RAIL_PRESSURE,       VAL_F32, 0.0, 5177.27 },
    { OBD_GET_FUEL_RAIL_GAUGE_PRESSURE, VAL_U32, 0.0, 655350.0 },
    { OBD_GET_COMMANDED_EGR,            VAL_F32, 0.0, 100.0 },
    { OBD_GET_EGR_ERROR,                VAL_F32, -100.0, 99.22 },
    { OBD_GET_EVAP_PURGE,               VAL_F32, 0.0, 100.0 },
    { OBD_GET_FUEL_LEVEL,               VAL_F32, 0.0, 100.0 },
    { OBD_GET_WARMUPS_SINCE_CLEAR,      VAL_U8,  0.0, 255.0 },
    { OBD_GET_DISTANCE_SINCE_CLEAR,     VAL_U16, 0.0, 65535.0 },
    { OBD_GET_EVAP_VAPOR_PRESSURE,      VAL_F32, -8192.0, 8191.75 },
    { OBD_GET_BAROMETRIC_PRESSURE,      VAL_U8,  0.0, 255.0 },
    { OBD_GET_CAT_TEMP_B1S1,            VAL_F32, -40.0, 6513.5 },
    { OBD_GET_CAT_TEMP_B2S1,            VAL_F32, -40.0, 6513.5 },
    { OBD_GET_CAT_TEMP_B1S2,            VAL_F32, -40.0, 6513.5 },
    { OBD_GET_CAT_TEMP_B2S2,            VAL_F32, -40.0, 6513.5 },
    { OBD_GET_MODULE_VOLTAGE,           VAL_F32, 0.0, 65.535 },
    { OBD_GET_ABSOLUTE_LOAD,            VAL_F32, 0.0, 25700.0 },
    { OBD_GET_RELATIVE_THROTTLE,        VAL_F32, 0.0, 100.0 },
    { OBD_GET_AMBIENT_AIR_TEMP,         VAL_I16, -40.0, 215.0 },
    { OBD_GET_TIME_WITH_MIL,            VAL_U16, 0.0, 65535.0 },
    { OBD_GET_TIME_SINCE_CLEAR,         VAL_U16, 0.0, 65535.0 },
    { OBD_GET_COMMANDED_LAMBDA,         VAL_F32, 0.0, 2.0 },
    { OBD_GET_THROTTLE_POSITION_B,      VAL_F32, 0.0, 100.0 },
    { OBD_GET_THROTTLE_POSITION_C,      VAL_F32, 0.0, 100.0 },
    { OBD_GET_PEDAL_POSITION_D,         VAL_F32, 0.0, 100.0 },
    { OBD_GET_PEDAL_POSITION_E,         VAL_F32, 0.0, 100.0 },
    { OBD_GET_PEDAL_POSITION_F,         VAL_F32, 0.0, 100.0 },
    { OBD_GET_THROTTLE_ACTUATOR,        VAL_F32, 0.0, 100.0 },
};

#define RANGE_CASES (sizeof(rangeCases) / sizeof(rangeCases[0]))

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static size_t valSize(valType_t type)
{
    static const size_t sizes[] = { sizeof(uint8_t), sizeof(uint16_t), sizeof(uint32_t), sizeof(int16_t), sizeof(float) };

    return sizes[type];
}

static double readValue(valType_t type, const uint8_t *response)
{
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    int16_t i16 = 0;
    float f32 = 0.0f;
    double value = 0.0;

    switch (type)
    {
        case VAL_U8:  memcpy(&u8, response, sizeof(u8));   value = u8;  break;
        case VAL_U16: memcpy(&u16, response, sizeof(u16)); value = u16; break;
        case VAL_U32: memcpy(&u32, response, sizeof(u32)); value = u32; break;
        case VAL_I16: memcpy(&i16, response, sizeof(i16)); value = i16; break;
        case VAL_F32: memcpy(&f32, response, sizeof(f32)); value = f32; break;
        default: break;
    }

    return value;
}

/* Data bytes the formula of 'pid' reads */
static uint8_t pidDataLen(uint8_t pid)
{
    size_t idx = 0;

    while (libobd2PidFormulas[idx].pid != pid)
    {
        idx++;
    }
    return libobd2PidFormulas[idx].dataLen;
}

static enum theft_alloc_res allocPidAnswer(struct theft *t, void *env, void **output)
{
    pidAnswer_t *a = calloc(1u, sizeof(*a));

    (void)env;
    if (a == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    a->caseIdx = (uint8_t)PBT_Range(t, 0u, RANGE_CASES - 1u);
    for (size_t idx = 0; idx < sizeof(a->data); idx++)
    {
        a->data[idx] = PBT_Byte(t);
    }
    *output = a;
    return THEFT_ALLOC_OK;
}

static void printPidAnswer(FILE *f, const void *instance, void *env)
{
    const pidAnswer_t *a = (const pidAnswer_t *)instance;

    (void)env;
    fprintf(f, "service %u, data %02X %02X %02X %02X", (unsigned)rangeCases[a->caseIdx].service,
            a->data[0], a->data[1], a->data[2], a->data[3]);
}

static enum theft_alloc_res allocDtcExchange(struct theft *t, void *env, void **output)
{
    dtcExchange_t *e = calloc(1u, sizeof(*e));

    (void)env;
    if (e == NULL)
    {
        return THEFT_ALLOC_ERROR;
    }
    e->msgCnt = (uint8_t)PBT_Range(t, 1u, MAX_DTC_MSGS);
    for (uint8_t msg = 0u; msg < e->msgCnt; msg++)
    {
        for (uint8_t slot = 0u; slot < DTC_SLOTS; slot++)
        {
            /* a third of the slots are fill */
            e->dtcs[msg][slot] = PBT_OneIn(t, 3u) ? 0u : (uint16_t)PBT_Range(t, 1u, 0xFFFFu);
        }
    }
    *output = e;
    return THEFT_ALLOC_OK;
}

static void printDtcExchange(FILE *f, const void *instance, void *env)
{
    const dtcExchange_t *e = (const dtcExchange_t *)instance;

    (void)env;
    for (uint8_t msg = 0u; msg < e->msgCnt; msg++)
    {
        fprintf(f, "[%04X %04X %04X] ", e->dtcs[msg][0], e->dtcs[msg][1], e->dtcs[msg][2]);
    }
}

static struct theft_type_info pidAnswerInfo   = PBT_TYPE_INFO(allocPidAnswer, printPidAnswer);
static struct theft_type_info dtcExchangeInfo = PBT_TYPE_INFO(allocDtcExchange, printDtcExchange);

/* Any data bytes of a $01 answer decode OK, to the service's type, inside the Annex B range */
static enum theft_trial_res propPidInRange(struct theft *t, void *arg)
{
    const pidAnswer_t *a = (const pidAnswer_t *)arg;
    const rangeCase_t *c = &rangeCases[a->caseIdx];
    const libobd2_data_t *request = &libobd2Services[c->service].serviceRequests[0];
    const uint8_t pid = request->data.param[0];
    const uint8_t dataLen = pidDataLen(pid);
    libobd2_data_t answer = { .data = { .sid = SID_SHOW_CURRENT_DATA + 0x40u, .param = { pid } }, .dataLen = 2u + dataLen };
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0u;
    obd_status_t status = {0};
    double value = 0.0;

    (void)t;
    memcpy(&answer.data.param[1], a->data, dataLen);
    status = LibOBD2_DecodeResponse(request, &answer, 1u, response, &responseLen);
    if ((OBD_STATUS_OK != status.response) || (valSize(c->type) != responseLen))
    {
        return THEFT_TRIAL_FAIL;
    }
    value = readValue(c->type, response);
    return ((value >= (c->min - RANGE_SLACK)) && (value <= (c->max + RANGE_SLACK))) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* Any DTC answers decode to exactly their non-zero DTCs, in order */
static enum theft_trial_res propDtcsInOrder(struct theft *t, void *arg)
{
    const dtcExchange_t *e = (const dtcExchange_t *)arg;
    const libobd2_data_t *request = &libobd2Services[OBD_GET_STORED_DTCS].serviceRequests[0];
    libobd2_data_t answers[MAX_DTC_MSGS] = {0};
    uint16_t expected[MAX_DTC_MSGS * DTC_SLOTS] = {0};
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t expectedCnt = 0u;
    size_t responseLen = 0u;
    obd_status_t status = {0};

    (void)t;
    for (uint8_t msg = 0u; msg < e->msgCnt; msg++)
    {
        answers[msg].data.sid = SID_SHOW_STORED_DTCS + 0x40u;
        answers[msg].dataLen  = 1u + (2u * DTC_SLOTS);
        for (uint8_t slot = 0u; slot < DTC_SLOTS; slot++)
        {
            answers[msg].data.param[2u * slot]      = (uint8_t)(e->dtcs[msg][slot] >> 8);
            answers[msg].data.param[(2u * slot) + 1u] = (uint8_t)(e->dtcs[msg][slot] & 0xFFu);
            if (0u != e->dtcs[msg][slot])
            {
                expected[expectedCnt++] = e->dtcs[msg][slot];
            }
        }
    }

    status = LibOBD2_DecodeResponse(request, answers, e->msgCnt, response, &responseLen);
    if ((OBD_STATUS_OK != status.response) || ((expectedCnt * sizeof(uint16_t)) != responseLen))
    {
        return THEFT_TRIAL_FAIL;
    }
    return (0 == memcmp(expected, response, responseLen)) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       Any $01 answer decodes inside its Annex B range
 * - description: Property test (theft): for every $01 service with a
 *                scalar result and any data bytes A..D, decoding is
 *                OK, writes exactly the service's type and the value
 *                lies within the min / max of ISO 15031-5 Annex B.
 * - steps:
 *     1. Generate a service and A..D, build 41 PID A..
 *     2. DecodeResponse: OK, len = size of the type, min <= v <= max
 ******************************************************************/
void test_pos_pbt_LIBOBD2_DecodeResponse_0(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: any $01 answer decodes inside its Annex B range",
        .prop1     = propPidInRange,
        .type_info = { &pidAnswerInfo },
        .trials    = PBT_TRIALS_FAST,
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}

/******************************************************************
 * - brief:       DTC answers decode to their DTCs, in order
 * - description: Property test (theft): for 1..6 $03 answers with any
 *                DTCs and $0000 fill anywhere, the response is every
 *                non-zero DTC in answer and slot order, nothing else.
 * - steps:
 *     1. Generate the answers
 *     2. DecodeResponse: OK, the non-zero DTCs as uint16_t, in order
 ******************************************************************/
void test_pos_pbt_LIBOBD2_DecodeResponse_1(void)
{
    struct theft_run_config cfg = {
        .name      = "LibOBD2: DTC answers decode to their DTCs, in order",
        .prop1     = propDtcsInOrder,
        .type_info = { &dtcExchangeInfo },
        .trials    = PBT_TRIALS_FAST,
    };
    PBT_ASSERT_HOLDS(&cfg, NULL);
}
