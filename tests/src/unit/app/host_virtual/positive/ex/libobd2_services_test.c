/*************************************************************************************************************
 * Every service of libobd2_sid_t against the simulated ECU: the ECU answers are the ones of its answer table
 * (ecu/app/ecu_libobd2_services.h), queued on the virtual K-line by APPFX_QueueEcuExchange().
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "libobd2_services_test.h"
#include "libobd2_fixture.h"
#include "iso15031_5.h"
#include "libobd2.h"
#include "libobd2_formulas.h"
#include "libobd2_services.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define FLOAT_TOLERANCE  0.01f
#define VIN_PAD_LEN      3u
#define CALID_LEN        16u
#define CVN_LEN          4u
#define F1_NOTE          "F1 (#62) cuts this ECU answer; if it is OK now, F1 is fixed: drop the F1 branch"

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* Type a service writes into the response */
typedef enum
{
    VAL_U8,
    VAL_U16,
    VAL_U32,
    VAL_I16,
    VAL_F32,
    VAL_BOOL,
} valType_t;

typedef struct
{
    libobd2_sid_t service;
    valType_t type;
    double value;   /* what the ECU table value decodes to */
} valueCase_t;

/* ============================================ LOCAL VARIABLES ============================================ */
extern const libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES];
extern void LibOBD2_O2TestScaling(uint8_t tid, float *scale, libobd2_unit_t *unit);

/* The decoded value of every $01 / $02 answer of the ECU table (see the comments there) */
static const valueCase_t valueCases[] =
{
    { OBD_GET_COOLANT_TEMP,             VAL_I16, 114.0 },
    { OBD_GET_PIDS_SUPPORTED_01_20,     VAL_U32, (double)0xFFFFFFFFu },
    { OBD_GET_PIDS_SUPPORTED_21_40,     VAL_U32, (double)0xFFFFFFFFu },
    { OBD_GET_PIDS_SUPPORTED_41_60,     VAL_U32, (double)0xFFFF0000u },
    { OBD_GET_FREEZE_DTC,               VAL_U16, (double)0x0143u },
    { OBD_GET_ENGINE_LOAD,              VAL_F32, 127.0 * 100.0 / 255.0 },
    { OBD_GET_SHORT_FUEL_TRIM_BANK1,    VAL_F32, 3.90625 },
    { OBD_GET_LONG_FUEL_TRIM_BANK1,     VAL_F32, -3.90625 },
    { OBD_GET_SHORT_FUEL_TRIM_BANK2,    VAL_F32, 0.0 },
    { OBD_GET_LONG_FUEL_TRIM_BANK2,     VAL_F32, -100.0 },
    { OBD_GET_FUEL_PRESSURE,            VAL_U16, 300.0 },
    { OBD_GET_INTAKE_MAP,               VAL_U8,  33.0 },
    { OBD_GET_ENGINE_RPM,               VAL_F32, 1726.25 },
    { OBD_GET_VEHICLE_SPEED,            VAL_U8,  50.0 },
    { OBD_GET_TIMING_ADVANCE,           VAL_F32, 10.5 },
    { OBD_GET_INTAKE_AIR_TEMP,          VAL_I16, -20.0 },
    { OBD_GET_MAF_FLOW_RATE,            VAL_F32, 5.0 },
    { OBD_GET_THROTTLE_POSITION,        VAL_F32, 20.0 },
    { OBD_GET_RUN_TIME,                 VAL_U16, 600.0 },
    { OBD_GET_DISTANCE_WITH_MIL,        VAL_U16, 42.0 },
    { OBD_GET_FUEL_RAIL_PRESSURE,       VAL_F32, 256.0 * 0.079 },
    { OBD_GET_FUEL_RAIL_GAUGE_PRESSURE, VAL_U32, 10000.0 },
    { OBD_GET_COMMANDED_EGR,            VAL_F32, 100.0 },
    { OBD_GET_EGR_ERROR,                VAL_F32, -50.0 },
    { OBD_GET_EVAP_PURGE,               VAL_F32, 0.0 },
    { OBD_GET_FUEL_LEVEL,               VAL_F32, 60.0 },
    { OBD_GET_WARMUPS_SINCE_CLEAR,      VAL_U8,  15.0 },
    { OBD_GET_DISTANCE_SINCE_CLEAR,     VAL_U16, 300.0 },
    { OBD_GET_EVAP_VAPOR_PRESSURE,      VAL_F32, -50.0 },
    { OBD_GET_BAROMETRIC_PRESSURE,      VAL_U8,  101.0 },
    { OBD_GET_CAT_TEMP_B1S1,            VAL_F32, 410.0 },
    { OBD_GET_CAT_TEMP_B2S1,            VAL_F32, 380.0 },
    { OBD_GET_CAT_TEMP_B1S2,            VAL_F32, 360.0 },
    { OBD_GET_CAT_TEMP_B2S2,            VAL_F32, -40.0 },
    { OBD_GET_MODULE_VOLTAGE,           VAL_F32, 14.0 },
    { OBD_GET_ABSOLUTE_LOAD,            VAL_F32, 20.0 },
    { OBD_GET_RELATIVE_THROTTLE,        VAL_F32, 40.0 },
    { OBD_GET_AMBIENT_AIR_TEMP,         VAL_I16, 10.0 },
    { OBD_GET_TIME_WITH_MIL,            VAL_U16, 60.0 },
    { OBD_GET_TIME_SINCE_CLEAR,         VAL_U16, 300.0 },
    { OBD_GET_SECONDARY_AIR_STATUS,     VAL_U8,  (double)LIBOBD2_AIR_OFF },
    { OBD_GET_O2_SENSORS_PRESENT,       VAL_U8,  (double)0x33u },
    { OBD_GET_O2_SENSORS_PRESENT_4BANKS,VAL_U8,  (double)0x33u },
    { OBD_GET_OBD_STANDARD,             VAL_U8,  6.0 },
    { OBD_GET_PTO_STATUS,               VAL_BOOL, 1.0 },
    { OBD_GET_COMMANDED_LAMBDA,         VAL_F32, 31457.0 * 2.0 / 65535.0 },
    { OBD_GET_THROTTLE_POSITION_B,      VAL_F32, 20.0 },
    { OBD_GET_THROTTLE_POSITION_C,      VAL_F32, 40.0 },
    { OBD_GET_PEDAL_POSITION_D,         VAL_F32, 26.0 * 100.0 / 255.0 },
    { OBD_GET_PEDAL_POSITION_E,         VAL_F32, 13.0 * 100.0 / 255.0 },
    { OBD_GET_PEDAL_POSITION_F,         VAL_F32, 0.0 },
    { OBD_GET_THROTTLE_ACTUATOR,        VAL_F32, 60.0 },
    { OBD_GET_MAX_MAF,                  VAL_U16, 500.0 },
    { OBD_GET_FF_DTC,                   VAL_U16, (double)0x0143u },
    { OBD_GET_FF_COOLANT_TEMP,          VAL_I16, 150.0 },
    { OBD_GET_FF_ENGINE_RPM,            VAL_F32, 3000.0 },
    { OBD_GET_FF_VEHICLE_SPEED,         VAL_U8,  120.0 },
    { OBD_GET_FF_ENGINE_LOAD,           VAL_F32, 60.0 },
    { OBD_GET_FF_SHORT_FUEL_TRIM_BANK1, VAL_F32, 0.0 },
    { OBD_GET_FF_LONG_FUEL_TRIM_BANK1,  VAL_F32, 3.90625 },
    { OBD_GET_FF_SHORT_FUEL_TRIM_BANK2, VAL_F32, -3.90625 },
    { OBD_GET_FF_LONG_FUEL_TRIM_BANK2,  VAL_F32, 0.0 },
    { OBD_GET_FF_INTAKE_MAP,            VAL_U8,  100.0 },
    { OBD_GET_FF_INTAKE_AIR_TEMP,       VAL_I16, 25.0 },
};

/* $01 / $02 services that return a struct: checked by test_pos_ex_LIBOBD2_Services_9 (and _1) */
static bool isStructService(size_t service)
{
    return (OBD_GET_MONITOR_STATUS == service) || (OBD_GET_FUEL_SYSTEM_STATUS == service) ||
           (OBD_GET_FF_FUEL_SYSTEM_STATUS == service) || (OBD_GET_MONITOR_STATUS_DRIVE_CYCLE == service) ||
           (OBD_GET_MAX_VALUES == service) ||
           ((OBD_GET_O2_B1S1 <= service) && (OBD_GET_WIDE_O2_I_B2S4 >= service));
}

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static size_t valSize(valType_t type)
{
    static const size_t sizes[] = { sizeof(uint8_t), sizeof(uint16_t), sizeof(uint32_t), sizeof(int16_t), sizeof(float), sizeof(bool) };

    return sizes[type];
}

/* Reads the value of 'type' the library wrote into response */
static double readValue(valType_t type, const uint8_t *response)
{
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    int16_t i16 = 0;
    float f32 = 0.0f;
    bool b = false;
    double value = 0.0;

    switch (type)
    {
        case VAL_U8:  memcpy(&u8, response, sizeof(u8));   value = u8;  break;
        case VAL_U16: memcpy(&u16, response, sizeof(u16)); value = u16; break;
        case VAL_U32: memcpy(&u32, response, sizeof(u32)); value = u32; break;
        case VAL_I16: memcpy(&i16, response, sizeof(i16)); value = i16; break;
        case VAL_F32: memcpy(&f32, response, sizeof(f32)); value = f32; break;
        case VAL_BOOL: memcpy(&b, response, sizeof(b));    value = b ? 1.0 : 0.0; break;
        default: break;
    }

    return value;
}

/* Runs 'service' against the simulated ECU */
static obd_status_t requestFromEcu(libobd2_sid_t service, bool readsUntilSilent, uint8_t *response, size_t *responseLen)
{
    APPFX_SetUpLine();
    if (readsUntilSilent)
    {
        APPFX_SuppressEcho();
    }
    (void)APPFX_QueueEcuExchange(&libobd2Services[service]);

    return LibOBD2_RequestService(&appFx.ctx, service, response, responseLen);
}

/* The uint16_t DTC at index idx of a DTC service response */
static uint16_t dtcAt(const uint8_t *response, size_t idx)
{
    uint16_t dtc = 0;

    memcpy(&dtc, &response[idx * sizeof(dtc)], sizeof(dtc));
    return dtc;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       Every $01 / $02 service decodes the ECU's value
 * - description: Each live data and freeze frame service is run
 *                against the simulated ECU; the response holds the
 *                service's type with the value of its Annex B
 *                formula. Every such service of libobd2_sid_t is
 *                covered. An answer F1 cuts (41 06 ..: the PID byte $06
 *                is the running sum of 48 6B 12 41) is expected to
 *                fail until F1 is fixed.
 * - steps:
 *     1. For each case: 68 6A F1 01 PID CS (or 02 PID 00), the ECU
 *        answers from its table
 *     2. OK, responseLen = size of the type, value as expected
 *     3. The cases cover every scalar service from coolant to the
 *        last freeze frame one (structs: _1 and _9)
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_0(void)
{
    char msg[64] = {0};
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};
    bool covered[OBD2_MAX_NUM_SERVICES] = {false};

    for (size_t idx = 0; idx < (sizeof(valueCases) / sizeof(valueCases[0])); idx++)
    {
        const valueCase_t *c = &valueCases[idx];

        (void)snprintf(msg, sizeof(msg), "service %u", (unsigned)c->service);
        memset(response, 0, sizeof(response));
        responseLen = 0;

        /* 1. */
        status = requestFromEcu(c->service, false, response, &responseLen);
        covered[c->service] = true;
        if (APPFX_EcuExchangeHitsF1(&libobd2Services[c->service]))
        {
            TEST_ASSERT_NOT_EQUAL_HEX16_MESSAGE(OBD_STATUS_OK, status.response, F1_NOTE);
            continue;
        }

        /* 2. */
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(OBD_STATUS_OK, status.response, msg);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(OBD_STATUS_OK, status.timeout, msg);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(valSize(c->type), responseLen, msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(FLOAT_TOLERANCE, (float)c->value, (float)readValue(c->type, response), msg);
    }

    /* 3. */
    for (size_t service = OBD_GET_COOLANT_TEMP; service <= OBD_GET_FF_INTAKE_AIR_TEMP; service++)
    {
        (void)snprintf(msg, sizeof(msg), "service %u has no case", (unsigned)service);
        TEST_ASSERT_TRUE_MESSAGE(covered[service] || isStructService(service), msg);
    }
}

/******************************************************************
 * - brief:       Monitor status and readiness ($01 $01, $01 $41)
 * - description: PID $01: A = MIL (bit 7) + DTC count, B = continuous
 *                monitors supported / incomplete, C D = non-continuous
 *                ones. PID $41 has the same B C D for this drive cycle.
 * - steps:
 *     1. Tester: 68 6A F1 01 01 CS     ECU: 48 6B 12 41 01 84 07 E5 00 CS
 *     2. MIL on, 4 DTCs; misfire, fuel and components supported; cat,
 *        evap, O2, O2 heater, EGR supported; all complete: ready
 *     3. PID $41: 00 07 E5 04: EVAP not complete this cycle, not ready
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_1(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};
    libobd2_monitor_status_t monitor = {0};

    /* 1. */
    status = requestFromEcu(OBD_GET_MONITOR_STATUS, false, response, &responseLen);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(monitor), responseLen);
    memcpy(&monitor, response, sizeof(monitor));
    TEST_ASSERT_TRUE(monitor.milOn);
    TEST_ASSERT_EQUAL_UINT8(4u, monitor.dtcCount);
    TEST_ASSERT_EQUAL_HEX8(LIBOBD2_MON_MISFIRE | LIBOBD2_MON_FUEL_SYSTEM | LIBOBD2_MON_COMPONENTS,
                           monitor.monitors.continuousSupported);
    TEST_ASSERT_EQUAL_HEX8(0x00u, monitor.monitors.continuousIncomplete);
    TEST_ASSERT_EQUAL_HEX8(LIBOBD2_MON_CATALYST | LIBOBD2_MON_EVAP | LIBOBD2_MON_O2_SENSOR | LIBOBD2_MON_O2_HEATER |
                           LIBOBD2_MON_EGR, monitor.monitors.nonContinuousSupported);
    TEST_ASSERT_EQUAL_HEX8(0x00u, monitor.monitors.nonContinuousIncomplete);
    TEST_ASSERT_TRUE(LibOBD2_IsReady(&monitor.monitors));

    /* 3. */
    status = requestFromEcu(OBD_GET_MONITOR_STATUS_DRIVE_CYCLE, false, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(libobd2_monitors_t), responseLen);
    memcpy(&monitor.monitors, response, sizeof(monitor.monitors));
    TEST_ASSERT_EQUAL_HEX8(LIBOBD2_MON_EVAP, monitor.monitors.nonContinuousIncomplete);
    TEST_ASSERT_FALSE(LibOBD2_IsReady(&monitor.monitors));
}

/******************************************************************
 * - brief:       VIN from the simulated ECU ($09 $02)
 * - description: The ECU reports the VIN the ISO 9141-2 way: count 5,
 *                message 1 = 3 fill bytes + the first character.
 * - steps:
 *     1. Tester: 09 01, ECU: 49 01 05; tester: 09 02, ECU: 5 messages
 *     2. OK, 20 bytes: 00 00 00 "WVWZZZ1JZXW000001"
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_2(void)
{
    const uint8_t pad[VIN_PAD_LEN] = {0};
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    /* 1. */
    UART_TRACE_Begin("test_pos_ex_LIBOBD2_Services_2.vcd");
    status = requestFromEcu(OBD_GET_VIN, false, response, &responseLen);
    UART_TRACE_End();

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(VIN_PAD_LEN + APPFX_VIN_LEN, responseLen);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(pad, response, VIN_PAD_LEN);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("WVWZZZ1JZXW000001", &response[VIN_PAD_LEN], APPFX_VIN_LEN);
}

/******************************************************************
 * - brief:       CALIDs and CVNs from the simulated ECU ($09 $04, $06)
 * - description: Two calibration IDs (8 messages) and their two CVNs
 *                (one message each), joined in order.
 * - steps:
 *     1. RequestService(CALID): OK, 32 bytes, both IDs, $00 padded
 *     2. RequestService(CVN): OK, 8 bytes, MSB first
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_3(void)
{
    const uint8_t calIds[2u * CALID_LEN] = { 'J', 'M', 'B', '*', '3', '6', '7', '6', '1', '5', '0', '0', 0, 0, 0, 0,
                                             'L', 'I', 'B', 'O', 'B', 'D', '2', '-', 'E', 'C', 'U', '-', 'S', 'I', 'M', 0 };
    const uint8_t cvns[2u * CVN_LEN] = { 0x17, 0x91, 0xBC, 0x82, 0x2A, 0xE0, 0x62, 0xBE };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    /* 1. */
    status = requestFromEcu(OBD_GET_CALID, false, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(calIds), responseLen);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(calIds, response, sizeof(calIds));

    /* 2. */
    status = requestFromEcu(OBD_GET_CVN, false, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(cvns), responseLen);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(cvns, response, sizeof(cvns));
}

/******************************************************************
 * - brief:       Stored DTCs over two answers ($03)
 * - description: The ECU sends 2 messages of 3 DTC slots; the tester
 *                reads until P2 max passes with no answer and returns
 *                the 4 non-zero DTCs in order, fill skipped.
 * - steps:
 *     1. Tester: 68 6A F1 03 CS
 *        ECU:    48 6B 12 43 01 43 C1 96 02 34 CS
 *                48 6B 12 43 81 23 00 00 00 00 CS, then silence
 *     2. OK, 4 x uint16_t: $0143 $C196 $0234 $8123
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_4(void)
{
    const uint16_t expected[] = { 0x0143u, 0xC196u, 0x0234u, 0x8123u };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    /* 1. */
    UART_TRACE_Begin("test_pos_ex_LIBOBD2_Services_4.vcd");
    status = requestFromEcu(OBD_GET_STORED_DTCS, true, response, &responseLen);
    UART_TRACE_End();

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), responseLen);
    for (size_t idx = 0; idx < (sizeof(expected) / sizeof(expected[0])); idx++)
    {
        TEST_ASSERT_EQUAL_HEX16(expected[idx], dtcAt(response, idx));
    }
}

/******************************************************************
 * - brief:       Pending ($07) and permanent ($0A) DTCs
 * - description: Same read-until-silent exchange, one message each.
 * - steps:
 *     1. RequestService(pending):   OK, P0300
 *     2. RequestService(permanent): OK, P0143
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_5(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    /* 1. */
    status = requestFromEcu(OBD_GET_PENDING_DTCS, true, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(uint16_t), responseLen);
    TEST_ASSERT_EQUAL_HEX16(0x0300u, dtcAt(response, 0u));

    /* 2. */
    status = requestFromEcu(OBD_GET_PERMANENT_DTCS, true, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(uint16_t), responseLen);
    TEST_ASSERT_EQUAL_HEX16(0x0143u, dtcAt(response, 0u));
}

/******************************************************************
 * - brief:       No DTCs: the ECU does not answer $03 at all
 * - description: On ISO 9141-2 an ECU without stored DTCs may stay
 *                silent (ISO 15031-5 6.3.1). That is 0 DTCs, not an
 *                error.
 * - steps:
 *     1. Tester: 68 6A F1 03 CS, nothing comes back
 *     2. OK, responseLen 0
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_6(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 1u;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    status = LibOBD2_RequestService(&appFx.ctx, OBD_GET_STORED_DTCS, response, &responseLen);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_size_t(0u, responseLen);
}

/******************************************************************
 * - brief:       Clear DTCs ($04)
 * - description: The ECU confirms with 44 and no data.
 * - steps:
 *     1. Tester: 68 6A F1 04 CS        ECU: 48 6B 12 44 CS
 *     2. OK, responseLen 0
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_7(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 1u;
    obd_status_t status = {0};

    /* 1. */
    status = requestFromEcu(OBD_CLEAR_DTCS, false, response, &responseLen);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(0u, responseLen);
}

/******************************************************************
 * - brief:       Every table service is answered by the simulated ECU
 * - description: Each service of libobd2_sid_t gets an OK answer from
 *                the ECU table, so the ECU covers the whole library.
 *                Answers F1 cuts fail until F1 is fixed.
 * - steps:
 *     1. For every service: RequestService against the ECU
 *     2. OK (not OK for an answer F1 cuts)
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_8(void)
{
    char msg[32] = {0};
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    for (size_t service = 0; service < OBD2_MAX_NUM_SERVICES; service++)
    {
        const bool readsUntilSilent = (OBD_LONG_SID == libobd2Services[service].sidSize) &&
                                      (TP_READ_UNTIL_TO == libobd2Services[service].transferProtocol[1]);

        (void)snprintf(msg, sizeof(msg), "service %u", (unsigned)service);

        /* 1. */
        status = requestFromEcu((libobd2_sid_t)service, readsUntilSilent, response, &responseLen);

        /* 2. */
        if (APPFX_EcuExchangeHitsF1(&libobd2Services[service]))
        {
            TEST_ASSERT_NOT_EQUAL_HEX16_MESSAGE(OBD_STATUS_OK, status.response, F1_NOTE);
            continue;
        }
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(OBD_STATUS_OK, status.response, msg);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(OBD_STATUS_OK, status.timeout, msg);
    }
}

/******************************************************************
 * - brief:       DTCs are displayed as ISO 15031-5 shows them
 * - description: Bits 15-14 pick P/C/B/U, bits 13-12 the first digit,
 *                the rest are three hex digits (Figure 15).
 * - steps:
 *     1. $0143 -> P0143, $C196 -> U0196, $8123 -> B0123,
 *        $4ABC -> C0ABC, $3FFF -> P3FFF
 ******************************************************************/
void test_pos_ex_LIBOBD2_DtcToString_0(void)
{
    char str[LIBOBD2_DTC_STR_LEN] = {0};

    LibOBD2_DtcToString(0x0143u, str);
    TEST_ASSERT_EQUAL_STRING("P0143", str);
    LibOBD2_DtcToString(0xC196u, str);
    TEST_ASSERT_EQUAL_STRING("U0196", str);
    LibOBD2_DtcToString(0x8123u, str);
    TEST_ASSERT_EQUAL_STRING("B0123", str);
    LibOBD2_DtcToString(0x4ABCu, str);
    TEST_ASSERT_EQUAL_STRING("C0ABC", str);
    LibOBD2_DtcToString(0x3FFFu, str);
    TEST_ASSERT_EQUAL_STRING("P3FFF", str);
}

/******************************************************************
 * - brief:       Services that return a struct
 * - description: Fuel system status ($01 and $02 $03), the
 *                conventional O2 sensors ($14-$1B: voltage, trim and
 *                whether the trim is used) and the wide-range ones
 *                ($24-$2B: lambda + voltage, $34-$3B: lambda + mA).
 * - steps:
 *     1. Fuel system: system 1 closed loop, no system 2 (both $01, $02)
 *     2. O2 B1S1: 41 14 8C 80 -> 0.7 V, 0 %, trim used
 *        O2 B1S2: 41 15 14 FF -> 0.1 V, trim not used
 *        O2 B2S1: 41 18 96 7B -> 0.75 V, -3.906 %
 *     3. Every wide-range sensor: 80 00 66 66 -> lambda 1.0, 3.2 V
 *        and 80 00 81 00 -> lambda 1.0, 1 mA
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_9(void)
{
    const libobd2_sid_t fuelSystems[] = { OBD_GET_FUEL_SYSTEM_STATUS, OBD_GET_FF_FUEL_SYSTEM_STATUS };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};
    libobd2_fuel_system_status_t fuel = {0};
    libobd2_o2_sensor_t o2 = {0};
    libobd2_o2_wide_voltage_t wideV = {0};
    libobd2_o2_wide_current_t wideI = {0};

    /* 1. */
    for (size_t idx = 0; idx < (sizeof(fuelSystems) / sizeof(fuelSystems[0])); idx++)
    {
        status = requestFromEcu(fuelSystems[idx], false, response, &responseLen);
        TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
        TEST_ASSERT_EQUAL_size_t(sizeof(fuel), responseLen);
        memcpy(&fuel, response, sizeof(fuel));
        TEST_ASSERT_EQUAL_HEX8(LIBOBD2_FUELSYS_CL, fuel.fuelSystem1);
        TEST_ASSERT_EQUAL_HEX8(0x00u, fuel.fuelSystem2);
    }

    /* 2. */
    status = requestFromEcu(OBD_GET_O2_B1S1, false, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(o2), responseLen);
    memcpy(&o2, response, sizeof(o2));
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.7f, o2.voltage);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.0f, o2.shortTermTrim);
    TEST_ASSERT_TRUE(o2.trimUsed);

    status = requestFromEcu(OBD_GET_O2_B1S2, false, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    memcpy(&o2, response, sizeof(o2));
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.1f, o2.voltage);
    TEST_ASSERT_FALSE(o2.trimUsed);

    status = requestFromEcu(OBD_GET_O2_B2S1, false, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    memcpy(&o2, response, sizeof(o2));
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.75f, o2.voltage);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, -3.90625f, o2.shortTermTrim);
    TEST_ASSERT_TRUE(o2.trimUsed);

    /* 3. */
    for (size_t service = OBD_GET_WIDE_O2_V_B1S1; service <= OBD_GET_WIDE_O2_V_B2S4; service++)
    {
        status = requestFromEcu((libobd2_sid_t)service, false, response, &responseLen);
        TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
        TEST_ASSERT_EQUAL_size_t(sizeof(wideV), responseLen);
        memcpy(&wideV, response, sizeof(wideV));
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 1.0f, wideV.lambda);
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 3.2f, wideV.voltage);
    }
    for (size_t service = OBD_GET_WIDE_O2_I_B1S1; service <= OBD_GET_WIDE_O2_I_B2S4; service++)
    {
        status = requestFromEcu((libobd2_sid_t)service, false, response, &responseLen);
        TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
        TEST_ASSERT_EQUAL_size_t(sizeof(wideI), responseLen);
        memcpy(&wideI, response, sizeof(wideI));
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 1.0f, wideI.lambda);
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 1.0f, wideI.current);
    }
}

/******************************************************************
 * - brief:       In-use performance tracking ($09 $08)
 * - description: Count 8, then 8 messages of 2 counters each; the
 *                response is the 16 counters, 2 bytes MSB first.
 * - steps:
 *     1. Tester: 09 07, ECU: 49 07 08; tester: 09 08, ECU: 8 messages
 *     2. OK, 32 bytes: OBDCOND 300, IGNCYCCNTR 600, CATCOMP1 100 ...
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_10(void)
{
    const uint8_t ipt[32] = { 0x01, 0x2C, 0x02, 0x58, 0x00, 0x64, 0x00, 0xC8, 0x00, 0x32, 0x00, 0x50,
                              0x00, 0x28, 0x00, 0x50, 0x00, 0x30, 0x00, 0x50, 0x00, 0x29, 0x00, 0x50,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};

    /* 1. */
    status = requestFromEcu(OBD_GET_IPT, false, response, &responseLen);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(ipt), responseLen);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ipt, response, sizeof(ipt));
}

/******************************************************************
 * - brief:       Full scales of PID $4F and LibOBD2_Rescale
 * - description: 41 4F 04 10 00 1E: lambda up to 4, O2 voltage up to
 *                16 V, current default, MAP up to 300 kPa. A wide-range
 *                value decoded with the default scale is rescaled.
 * - steps:
 *     1. RequestService(max values): 4, 16, 0, 300
 *     2. Rescale(1.0 lambda, default 2) with 4 -> 2.0; with 0 -> 1.0
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_11(void)
{
    uint8_t response[APPFX_RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};
    libobd2_max_values_t maxValues = {0};

    /* 1. */
    status = requestFromEcu(OBD_GET_MAX_VALUES, false, response, &responseLen);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(sizeof(maxValues), responseLen);
    memcpy(&maxValues, response, sizeof(maxValues));
    TEST_ASSERT_EQUAL_UINT8(4u, maxValues.maxLambda);
    TEST_ASSERT_EQUAL_UINT8(16u, maxValues.maxO2Voltage);
    TEST_ASSERT_EQUAL_UINT8(0u, maxValues.maxO2Current);
    TEST_ASSERT_EQUAL_UINT16(300u, maxValues.maxMap);

    /* 2. */
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 2.0f, LibOBD2_Rescale(1.0f, LIBOBD2_DEFAULT_MAX_LAMBDA, maxValues.maxLambda));
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 1.0f, LibOBD2_Rescale(1.0f, LIBOBD2_DEFAULT_MAX_O2_MA, maxValues.maxO2Current));
}

/* Queues the simulated ECU's answers to one direct request */
static void queueEcuRequest(const obd_data_t *request, size_t requestLen)
{
    libobd2_service_t service = {0};

    service.serviceRequests[0].data    = *request;
    service.serviceRequests[0].dataLen = requestLen;
    (void)APPFX_QueueEcuExchange(&service);
}

/******************************************************************
 * - brief:       Raw passthrough: every answer of any request
 * - description: LibOBD2_RequestRaw returns the answers as received,
 *                however many ECUs answer, and OK with none.
 * - steps:
 *     1. Raw 01 0C to the simulated ECU: one answer 41 0C 1A F9
 *     2. Raw 01 00 answered by two ECUs: both answers, in order
 *     3. Raw 01 FF, no ECU answers: OK, 0 answers
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_12(void)
{
    const obd_data_t rpm = { .sid = SID_SHOW_CURRENT_DATA, .param = { PID_01_ENGINE_RPM } };
    const obd_data_t pids = { .sid = SID_SHOW_CURRENT_DATA, .param = { PID_01_PIDS_SUPPORTED_01_20 } };
    const obd_data_t unknown = { .sid = SID_SHOW_CURRENT_DATA, .param = { 0xFFu } };
    const uint8_t ecu1[] = { 0x41, 0x00, 0xBE, 0x3F, 0xA8, 0x13 };
    const uint8_t ecu2[] = { 0x41, 0x00, 0x98, 0x18, 0x00, 0x01 };
    libobd2_data_t answers[4] = {0};
    size_t answerCnt = 0;
    obd_status_t status = {0};
    uint32_t time = 0;

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    queueEcuRequest(&rpm, 2u);
    status = LibOBD2_RequestRaw(&appFx.ctx, &rpm, 2u, answers, 4u, &answerCnt);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(1u, answerCnt);
    TEST_ASSERT_EQUAL_size_t(4u, answers[0].dataLen);
    TEST_ASSERT_EQUAL_HEX8(0x41u, answers[0].data.sid);
    TEST_ASSERT_EQUAL_HEX8(0x1Au, answers[0].data.param[1]);
    TEST_ASSERT_EQUAL_HEX8(0xF9u, answers[0].data.param[2]);

    /* 2. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    time = APPFX_QueueEcuFrame(APPFX_REQ_DURATION(2u), ecu1, sizeof(ecu1));
    (void)APPFX_QueueEcuFrame(time, ecu2, sizeof(ecu2));
    status = LibOBD2_RequestRaw(&appFx.ctx, &pids, 2u, answers, 4u, &answerCnt);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(2u, answerCnt);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ecu1, (const uint8_t *)&answers[0].data, sizeof(ecu1));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ecu2, (const uint8_t *)&answers[1].data, sizeof(ecu2));

    /* 3. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    status = LibOBD2_RequestRaw(&appFx.ctx, &unknown, 2u, answers, 4u, &answerCnt);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(0u, answerCnt);
}

/******************************************************************
 * - brief:       Supported TIDs of $05, $06 and $08
 * - description: The bitmask of the range $01-$20, read the way each
 *                service asks for it (05 00 O2S, 06 00, 08 00 + 5 x 00).
 * - steps:
 *     1. $05 for B1S1: TIDs 1, 2, 3, 5 -> $E8000000
 *     2. $06: TID 1 -> $80000000
 *     3. $08: TID 1 -> $80000000
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_13(void)
{
    const obd_data_t req05 = { .sid = SID_TEST_RESULTS_O2_SENSORS, .param = { 0x00u, 0x01u } };
    const obd_data_t req06 = { .sid = SID_TEST_RESULTS_OTHER, .param = { 0x00u } };
    const obd_data_t req08 = { .sid = SID_CONTROL_ONBOARD_SYSTEM, .param = { 0x00u } };
    uint32_t supported = 0;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    queueEcuRequest(&req05, 3u);
    status = LibOBD2_RequestSupportedTids(&appFx.ctx, SID_TEST_RESULTS_O2_SENSORS, 0x00u, 0x01u, &supported);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX32(0xE8000000u, supported);

    /* 2. */
    APPFX_SetUpLine();
    queueEcuRequest(&req06, 2u);
    status = LibOBD2_RequestSupportedTids(&appFx.ctx, SID_TEST_RESULTS_OTHER, 0x00u, 0x00u, &supported);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX32(0x80000000u, supported);

    /* 3. */
    APPFX_SetUpLine();
    queueEcuRequest(&req08, 7u);
    status = LibOBD2_RequestSupportedTids(&appFx.ctx, SID_CONTROL_ONBOARD_SYSTEM, 0x00u, 0x00u, &supported);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX32(0x80000000u, supported);
}

/******************************************************************
 * - brief:       $05 O2 sensor test results, scaled by Annex C
 * - description: TID $01 is a constant (value only), TID $05 a
 *                calculated switch time with min and max limits.
 * - steps:
 *     1. 05 01 01 -> 45 01 01 5A: 0.45 V, no limits, passed
 *     2. 05 05 01 -> 45 05 01 19 0A 32: 0.1 s in 0.04..0.2 s, passed
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_14(void)
{
    const obd_data_t req01 = { .sid = SID_TEST_RESULTS_O2_SENSORS, .param = { 0x01u, 0x01u } };
    const obd_data_t req05 = { .sid = SID_TEST_RESULTS_O2_SENSORS, .param = { 0x05u, 0x01u } };
    libobd2_o2_test_t result = {0};
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    queueEcuRequest(&req01, 3u);
    status = LibOBD2_RequestO2SensorTest(&appFx.ctx, 0x01u, 0x01u, &result);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_FALSE(result.hasLimits);
    TEST_ASSERT_TRUE(result.passed);
    TEST_ASSERT_EQUAL_INT(LIBOBD2_UNIT_VOLT, result.unit);
    TEST_ASSERT_EQUAL_HEX8(0x5Au, result.rawValue);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.45f, result.value);

    /* 2. */
    APPFX_SetUpLine();
    queueEcuRequest(&req05, 3u);
    status = LibOBD2_RequestO2SensorTest(&appFx.ctx, 0x05u, 0x01u, &result);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_TRUE(result.hasLimits);
    TEST_ASSERT_TRUE(result.passed);
    TEST_ASSERT_EQUAL_INT(LIBOBD2_UNIT_SECOND, result.unit);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.1f, result.value);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.04f, result.min);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOLERANCE, 0.2f, result.max);
}

/******************************************************************
 * - brief:       $06 monitor test results, all components and limits
 * - description: One TID answered three times: component 1 with a max
 *                and a min limit, component 2 with a max it exceeds.
 * - steps:
 *     1. 06 01, ECU: 46 01 01 0064 00C8 / 46 01 81 0064 0032 /
 *        46 01 02 012C 00C8, then silence
 *     2. 3 results: pass (max), pass (min), fail (max)
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_15(void)
{
    const obd_data_t req = { .sid = SID_TEST_RESULTS_OTHER, .param = { 0x01u } };
    libobd2_monitor_test_t results[4] = {0};
    size_t resultCnt = 0;
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    APPFX_SuppressEcho();
    queueEcuRequest(&req, 2u);
    status = LibOBD2_RequestMonitorTests(&appFx.ctx, 0x01u, results, 4u, &resultCnt);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_size_t(3u, resultCnt);
    TEST_ASSERT_EQUAL_UINT8(1u, results[0].componentId);
    TEST_ASSERT_FALSE(results[0].limitIsMin);
    TEST_ASSERT_EQUAL_UINT16(100u, results[0].value);
    TEST_ASSERT_EQUAL_UINT16(200u, results[0].limit);
    TEST_ASSERT_TRUE(results[0].passed);
    TEST_ASSERT_EQUAL_UINT8(1u, results[1].componentId);
    TEST_ASSERT_TRUE(results[1].limitIsMin);
    TEST_ASSERT_EQUAL_UINT16(50u, results[1].limit);
    TEST_ASSERT_TRUE(results[1].passed);
    TEST_ASSERT_EQUAL_UINT8(2u, results[2].componentId);
    TEST_ASSERT_EQUAL_UINT16(300u, results[2].value);
    TEST_ASSERT_FALSE(results[2].passed);
}

/******************************************************************
 * - brief:       $08 request control: evaporative system leak test
 * - description: 08 01 00 00 00 00 00 is answered 48 01 00 00 00 00 00.
 * - steps:
 *     1. RequestControl(TID 1, 5 x 00)
 *     2. OK, DATA_A..E = 00
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_16(void)
{
    const obd_data_t req = { .sid = SID_CONTROL_ONBOARD_SYSTEM, .param = { 0x01u } };
    const uint8_t dataIn[LIBOBD2_CONTROL_DATA_LEN] = {0};
    uint8_t dataOut[LIBOBD2_CONTROL_DATA_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    queueEcuRequest(&req, 7u);
    status = LibOBD2_RequestControl(&appFx.ctx, 0x01u, dataIn, dataOut);

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(dataIn, dataOut, LIBOBD2_CONTROL_DATA_LEN);
}


/******************************************************************
 * - brief:       $05 TID scaling of every Annex C range
 * - description: Each TID range gets its scaling per bit and unit;
 *                reserved and manufacturer TIDs are left raw (1, no
 *                unit). Both ends of every range are checked.
 * - steps:
 *     1. O2TestScaling for every range bound and the gaps between
 *     2. scale and unit as in ISO 15031-5 Annex C
 ******************************************************************/
void test_pos_ex_LIBOBD2_O2TestScaling_0(void)
{
    static const struct { uint8_t tid; float scale; libobd2_unit_t unit; } cases[] =
    {
        { 0x01u, 0.005f, LIBOBD2_UNIT_VOLT },   { 0x04u, 0.005f, LIBOBD2_UNIT_VOLT },
        { 0x07u, 0.005f, LIBOBD2_UNIT_VOLT },   { 0x08u, 0.005f, LIBOBD2_UNIT_VOLT },
        { 0x41u, 0.005f, LIBOBD2_UNIT_VOLT },   { 0x4Fu, 0.005f, LIBOBD2_UNIT_VOLT },
        { 0x05u, 0.004f, LIBOBD2_UNIT_SECOND }, { 0x06u, 0.004f, LIBOBD2_UNIT_SECOND },
        { 0x21u, 0.004f, LIBOBD2_UNIT_SECOND }, { 0x2Fu, 0.004f, LIBOBD2_UNIT_SECOND },
        { 0x09u, 0.04f,  LIBOBD2_UNIT_SECOND }, { 0x0Au, 0.04f,  LIBOBD2_UNIT_SECOND },
        { 0x30u, 0.04f,  LIBOBD2_UNIT_SECOND }, { 0x3Fu, 0.04f,  LIBOBD2_UNIT_SECOND },
        { 0x50u, 0.05f,  LIBOBD2_UNIT_VOLT },   { 0x5Fu, 0.05f,  LIBOBD2_UNIT_VOLT },
        { 0x61u, 0.1f,   LIBOBD2_UNIT_HERTZ },  { 0x6Fu, 0.1f,   LIBOBD2_UNIT_HERTZ },
        { 0x70u, 1.0f,   LIBOBD2_UNIT_COUNT },  { 0x7Fu, 1.0f,   LIBOBD2_UNIT_COUNT },
        { 0x00u, 1.0f,   LIBOBD2_UNIT_NONE },   { 0x0Bu, 1.0f,   LIBOBD2_UNIT_NONE },
        { 0x1Fu, 1.0f,   LIBOBD2_UNIT_NONE },   { 0x20u, 1.0f,   LIBOBD2_UNIT_NONE },
        { 0x40u, 1.0f,   LIBOBD2_UNIT_NONE },   { 0x60u, 1.0f,   LIBOBD2_UNIT_NONE },
        { 0x80u, 1.0f,   LIBOBD2_UNIT_NONE },   { 0xFFu, 1.0f,   LIBOBD2_UNIT_NONE },
    };
    char msg[16] = {0};
    float scale = 0.0f;
    libobd2_unit_t unit = LIBOBD2_UNIT_NONE;

    for (size_t idx = 0; idx < (sizeof(cases) / sizeof(cases[0])); idx++)
    {
        /* 1. */
        (void)snprintf(msg, sizeof(msg), "TID $%02X", cases[idx].tid);
        LibOBD2_O2TestScaling(cases[idx].tid, &scale, &unit);

        /* 2. */
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0001f, cases[idx].scale, scale, msg);
        TEST_ASSERT_EQUAL_INT_MESSAGE(cases[idx].unit, unit, msg);
    }
}

/******************************************************************
 * - brief:       $05 results outside their limits fail
 * - description: TID $05 with min $0A and max $32: a value below the
 *                min and one above the max are both reported as not
 *                passed.
 * - steps:
 *     1. 05 05 01 -> 45 05 01 05 0A 32: 0.02 s < 0.04 s, failed
 *     2. 05 05 01 -> 45 05 01 40 0A 32: 0.256 s > 0.2 s, failed
 ******************************************************************/
void test_pos_ex_LIBOBD2_Services_17(void)
{
    const uint8_t belowMin[] = { 0x45u, 0x05u, 0x01u, 0x05u, 0x0Au, 0x32u };
    const uint8_t aboveMax[] = { 0x45u, 0x05u, 0x01u, 0x40u, 0x0Au, 0x32u };
    libobd2_o2_test_t result = {0};
    obd_status_t status = {0};

    /* 1. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(3u), belowMin, sizeof(belowMin));
    status = LibOBD2_RequestO2SensorTest(&appFx.ctx, 0x05u, 0x01u, &result);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_TRUE(result.hasLimits);
    TEST_ASSERT_FALSE(result.passed);

    /* 2. */
    APPFX_SetUpLine();
    (void)APPFX_QueueEcuFrame(APPFX_REQ_DURATION(3u), aboveMax, sizeof(aboveMax));
    status = LibOBD2_RequestO2SensorTest(&appFx.ctx, 0x05u, 0x01u, &result);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_TRUE(result.hasLimits);
    TEST_ASSERT_FALSE(result.passed);
}
