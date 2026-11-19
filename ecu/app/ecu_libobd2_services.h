#ifndef ECU_LIBOBD2_SERVICES_H
#define ECU_LIBOBD2_SERVICES_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include <stdint.h>
#include "ecu_libobd2_service_requests.h"
#include "ecu_libobd2_transfer_protocol.h"
#include "libobd2_service_requests.h"

/* ================================================= MACROS ================================================ */
#define OBD_SHORT_SID ((libobd2_sid_t)(0xA5U))
#define OBD_LONG_SID ((libobd2_sid_t)(0xC3))

/* Answer table size, indexed [sid][pid]: every service up to $0A, every PID up to the last one answered.
 * Requests outside it get no answer, as an ISO 9141-2 ECU does for what it does not support. */
#define ECU_MAX_SID (SID_PERMANENT_DTCS + 1U)
#define ECU_MAX_PID (PID_01_MAX_VALUE_MAF + 1U)

/* Requests without a PID ($03, $04, $07, $0A) are found at pid 0, $05 / $06 / $08 at their TID (the O2
 * sensor byte of $05 is not looked at). Values in the comments are what the tester decodes. */
#define ECU_SRV_RESP \
    [SID_SHOW_CURRENT_DATA] = \
    { \
        [PID_01_PIDS_SUPPORTED_01_20]         = ECU_LIVE(PID_01_PIDS_SUPPORTED_01_20, 0xFF, 0xFF, 0xFF, 0xFF), /* 01-20, next range */ \
        [PID_01_MONITOR_STATUS]               = ECU_LIVE(PID_01_MONITOR_STATUS, 0x84, 0x07, 0xE5, 0x00), /* MIL on, 4 DTCs, all ready */ \
        [PID_01_FREEZE_DTC]                   = ECU_LIVE(PID_01_FREEZE_DTC, 0x01, 0x43),        /* P0143 */ \
        [PID_01_FUEL_SYSTEM_STATUS]           = ECU_LIVE(PID_01_FUEL_SYSTEM_STATUS, 0x02, 0x00), /* CL, no system 2 */ \
        [PID_01_ENGINE_LOAD]                  = ECU_LIVE(PID_01_ENGINE_LOAD, 0x7F),             /* 49.8 % */ \
        [PID_01_COOLANT_TEMP]                 = ECU_LIVE(PID_01_COOLANT_TEMP, 0x9A),            /* 114 degC */ \
        [PID_01_SHORT_TERM_FUEL_TRIM_BANK1]   = ECU_LIVE(PID_01_SHORT_TERM_FUEL_TRIM_BANK1, 0x85), /* 3.906 % */ \
        [PID_01_LONG_TERM_FUEL_TRIM_BANK1]    = ECU_LIVE(PID_01_LONG_TERM_FUEL_TRIM_BANK1, 0x7B),  /* -3.906 % */ \
        [PID_01_SHORT_TERM_FUEL_TRIM_BANK2]   = ECU_LIVE(PID_01_SHORT_TERM_FUEL_TRIM_BANK2, 0x80), /* 0 % */ \
        [PID_01_LONG_TERM_FUEL_TRIM_BANK2]    = ECU_LIVE(PID_01_LONG_TERM_FUEL_TRIM_BANK2, 0x00),  /* -100 % */ \
        [PID_01_FUEL_PRESSURE]                = ECU_LIVE(PID_01_FUEL_PRESSURE, 0x64),           /* 300 kPa */ \
        [PID_01_INTAKE_MAP]                   = ECU_LIVE(PID_01_INTAKE_MAP, 0x21),              /* 33 kPa */ \
        [PID_01_ENGINE_RPM]                   = ECU_LIVE(PID_01_ENGINE_RPM, 0x1A, 0xF9),        /* 1726.25 rpm */ \
        [PID_01_VEHICLE_SPEED]                = ECU_LIVE(PID_01_VEHICLE_SPEED, 0x32),           /* 50 km/h */ \
        [PID_01_TIMING_ADVANCE]               = ECU_LIVE(PID_01_TIMING_ADVANCE, 0x95),          /* 10.5 deg */ \
        [PID_01_INTAKE_AIR_TEMP]              = ECU_LIVE(PID_01_INTAKE_AIR_TEMP, 0x14),         /* -20 degC */ \
        [PID_01_MAF_FLOW_RATE]                = ECU_LIVE(PID_01_MAF_FLOW_RATE, 0x01, 0xF4),     /* 5 g/s */ \
        [PID_01_THROTTLE_POSITION]            = ECU_LIVE(PID_01_THROTTLE_POSITION, 0x33),       /* 20 % */ \
        [PID_01_SECONDARY_AIR_STATUS]         = ECU_LIVE(PID_01_SECONDARY_AIR_STATUS, 0x04),    /* off */ \
        [PID_01_O2_SENSORS_PRESENT]           = ECU_LIVE(PID_01_O2_SENSORS_PRESENT, 0x33),      /* B1S1 B1S2 B2S1 B2S2 */ \
        [PID_01_O2_SENSOR_1_BANK1]            = ECU_LIVE(PID_01_O2_SENSOR_1_BANK1, 0x8C, 0x80), /* 0.7 V, 0 % */ \
        [PID_01_O2_SENSOR_2_BANK1]            = ECU_LIVE(PID_01_O2_SENSOR_2_BANK1, 0x14, 0xFF), /* 0.1 V, no trim */ \
        [PID_01_O2_SENSOR_3_BANK1]            = ECU_LIVE(PID_01_O2_SENSOR_3_BANK1, 0x5A, 0x85), /* 0.45 V, 3.906 % */ \
        [PID_01_O2_SENSOR_4_BANK1]            = ECU_LIVE(PID_01_O2_SENSOR_4_BANK1, 0x00, 0xFF), \
        [PID_01_O2_SENSOR_1_BANK2]            = ECU_LIVE(PID_01_O2_SENSOR_1_BANK2, 0x96, 0x7B), /* 0.75 V, -3.906 % */ \
        [PID_01_O2_SENSOR_2_BANK2]            = ECU_LIVE(PID_01_O2_SENSOR_2_BANK2, 0x28, 0xFF), /* 0.2 V, no trim */ \
        [PID_01_O2_SENSOR_3_BANK2]            = ECU_LIVE(PID_01_O2_SENSOR_3_BANK2, 0x00, 0xFF), \
        [PID_01_O2_SENSOR_4_BANK2]            = ECU_LIVE(PID_01_O2_SENSOR_4_BANK2, 0x00, 0xFF), \
        [PID_01_OBD_STANDARDS]                = ECU_LIVE(PID_01_OBD_STANDARDS, 0x06),           /* EOBD */ \
        [PID_01_O2_SENSORS_PRESENT_4BANKS]    = ECU_LIVE(PID_01_O2_SENSORS_PRESENT_4BANKS, 0x33), \
        [PID_01_AUX_INPUT_STATUS]             = ECU_LIVE(PID_01_AUX_INPUT_STATUS, 0x01),        /* PTO on */ \
        [PID_01_RUN_TIME_ENGINE_START]        = ECU_LIVE(PID_01_RUN_TIME_ENGINE_START, 0x02, 0x58), /* 600 s */ \
        [PID_01_PIDS_SUPPORTED_21_40]         = ECU_LIVE(PID_01_PIDS_SUPPORTED_21_40, 0xFF, 0xFF, 0xFF, 0xFF), /* 21-40, next range */ \
        [PID_01_DISTANCE_WITH_MIL]            = ECU_LIVE(PID_01_DISTANCE_WITH_MIL, 0x00, 0x2A), /* 42 km */ \
        [PID_01_FUEL_RAIL_PRESSURE]           = ECU_LIVE(PID_01_FUEL_RAIL_PRESSURE, 0x01, 0x00), /* 20.224 kPa */ \
        [PID_01_FUEL_RAIL_GAUGE_PRESSURE]     = ECU_LIVE(PID_01_FUEL_RAIL_GAUGE_PRESSURE, 0x03, 0xE8), /* 10000 kPa */ \
        [PID_01_O2_SENSOR_1_BANK1_FAER] = ECU_LIVE(PID_01_O2_SENSOR_1_BANK1_FAER, 0x80, 0x00, 0x66, 0x66), /* lambda 1.0, 3.2 V */ \
        [PID_01_O2_SENSOR_2_BANK1_FAER] = ECU_LIVE(PID_01_O2_SENSOR_2_BANK1_FAER, 0x80, 0x00, 0x66, 0x66), /* lambda 1.0, 3.2 V */ \
        [PID_01_O2_SENSOR_3_BANK1_FAER] = ECU_LIVE(PID_01_O2_SENSOR_3_BANK1_FAER, 0x80, 0x00, 0x66, 0x66), /* lambda 1.0, 3.2 V */ \
        [PID_01_O2_SENSOR_4_BANK1_FAER] = ECU_LIVE(PID_01_O2_SENSOR_4_BANK1_FAER, 0x80, 0x00, 0x66, 0x66), /* lambda 1.0, 3.2 V */ \
        [PID_01_O2_SENSOR_1_BANK2_FAER] = ECU_LIVE(PID_01_O2_SENSOR_1_BANK2_FAER, 0x80, 0x00, 0x66, 0x66), /* lambda 1.0, 3.2 V */ \
        [PID_01_O2_SENSOR_2_BANK2_FAER] = ECU_LIVE(PID_01_O2_SENSOR_2_BANK2_FAER, 0x80, 0x00, 0x66, 0x66), /* lambda 1.0, 3.2 V */ \
        [PID_01_O2_SENSOR_3_BANK2_FAER] = ECU_LIVE(PID_01_O2_SENSOR_3_BANK2_FAER, 0x80, 0x00, 0x66, 0x66), /* lambda 1.0, 3.2 V */ \
        [PID_01_O2_SENSOR_4_BANK2_FAER] = ECU_LIVE(PID_01_O2_SENSOR_4_BANK2_FAER, 0x80, 0x00, 0x66, 0x66), /* lambda 1.0, 3.2 V */ \
        [PID_01_COMMANDED_EGR]                = ECU_LIVE(PID_01_COMMANDED_EGR, 0xFF),           /* 100 % */ \
        [PID_01_EGR_ERROR]                    = ECU_LIVE(PID_01_EGR_ERROR, 0x40),               /* -50 % */ \
        [PID_01_COMMANDED_EVAP_PURGE]         = ECU_LIVE(PID_01_COMMANDED_EVAP_PURGE, 0x00),    /* 0 % */ \
        [PID_01_FUEL_TANK_LEVEL]              = ECU_LIVE(PID_01_FUEL_TANK_LEVEL, 0x99),         /* 60 % */ \
        [PID_01_WARMUPS_SINCE_CODES_CLEARED]  = ECU_LIVE(PID_01_WARMUPS_SINCE_CODES_CLEARED, 0x0F), /* 15 */ \
        [PID_01_DISTANCE_SINCE_CODES_CLEARED] = ECU_LIVE(PID_01_DISTANCE_SINCE_CODES_CLEARED, 0x01, 0x2C), /* 300 km */ \
        [PID_01_EVAP_SYSTEM_VAPOR_PRESSURE]   = ECU_LIVE(PID_01_EVAP_SYSTEM_VAPOR_PRESSURE, 0xFF, 0x38), /* -50 Pa */ \
        [PID_01_ABSOLUTE_BAROMETRIC_PRESSURE] = ECU_LIVE(PID_01_ABSOLUTE_BAROMETRIC_PRESSURE, 0x65), /* 101 kPa */ \
        [PID_01_O2_SENSOR_1_BANK1_FAER_CURRENT] = ECU_LIVE(PID_01_O2_SENSOR_1_BANK1_FAER_CURRENT, 0x80, 0x00, 0x81, 0x00), /* lambda 1.0, 1 mA */ \
        [PID_01_O2_SENSOR_2_BANK1_FAER_CURRENT] = ECU_LIVE(PID_01_O2_SENSOR_2_BANK1_FAER_CURRENT, 0x80, 0x00, 0x81, 0x00), /* lambda 1.0, 1 mA */ \
        [PID_01_O2_SENSOR_3_BANK1_FAER_CURRENT] = ECU_LIVE(PID_01_O2_SENSOR_3_BANK1_FAER_CURRENT, 0x80, 0x00, 0x81, 0x00), /* lambda 1.0, 1 mA */ \
        [PID_01_O2_SENSOR_4_BANK1_FAER_CURRENT] = ECU_LIVE(PID_01_O2_SENSOR_4_BANK1_FAER_CURRENT, 0x80, 0x00, 0x81, 0x00), /* lambda 1.0, 1 mA */ \
        [PID_01_O2_SENSOR_1_BANK2_FAER_CURRENT] = ECU_LIVE(PID_01_O2_SENSOR_1_BANK2_FAER_CURRENT, 0x80, 0x00, 0x81, 0x00), /* lambda 1.0, 1 mA */ \
        [PID_01_O2_SENSOR_2_BANK2_FAER_CURRENT] = ECU_LIVE(PID_01_O2_SENSOR_2_BANK2_FAER_CURRENT, 0x80, 0x00, 0x81, 0x00), /* lambda 1.0, 1 mA */ \
        [PID_01_O2_SENSOR_3_BANK2_FAER_CURRENT] = ECU_LIVE(PID_01_O2_SENSOR_3_BANK2_FAER_CURRENT, 0x80, 0x00, 0x81, 0x00), /* lambda 1.0, 1 mA */ \
        [PID_01_O2_SENSOR_4_BANK2_FAER_CURRENT] = ECU_LIVE(PID_01_O2_SENSOR_4_BANK2_FAER_CURRENT, 0x80, 0x00, 0x81, 0x00), /* lambda 1.0, 1 mA */ \
        [PID_01_CAT_TEMP_BANK1_SENSOR1]       = ECU_LIVE(PID_01_CAT_TEMP_BANK1_SENSOR1, 0x11, 0x94), /* 410 degC */ \
        [PID_01_CAT_TEMP_BANK2_SENSOR1]       = ECU_LIVE(PID_01_CAT_TEMP_BANK2_SENSOR1, 0x10, 0x68), /* 380 degC */ \
        [PID_01_CAT_TEMP_BANK1_SENSOR2]       = ECU_LIVE(PID_01_CAT_TEMP_BANK1_SENSOR2, 0x0F, 0xA0), /* 360 degC */ \
        [PID_01_CAT_TEMP_BANK2_SENSOR2]       = ECU_LIVE(PID_01_CAT_TEMP_BANK2_SENSOR2, 0x00, 0x00), /* -40 degC */ \
        [PID_01_PIDS_SUPPORTED_41_60]         = ECU_LIVE(PID_01_PIDS_SUPPORTED_41_60, 0xFF, 0xFF, 0x00, 0x00), /* 41-50 */ \
        [PID_01_MONITOR_STATUS_DRIVE_CYCLE]   = ECU_LIVE(PID_01_MONITOR_STATUS_DRIVE_CYCLE, 0x00, 0x07, 0xE5, 0x04), /* EVAP not done */ \
        [PID_01_FUEL_AIR_COMMANDED_EQUIV_RATIO] = ECU_LIVE(PID_01_FUEL_AIR_COMMANDED_EQUIV_RATIO, 0x7A, 0xE1), /* 0.96 */ \
        [PID_01_ABSOLUTE_THROTTLE_POSITION_B] = ECU_LIVE(PID_01_ABSOLUTE_THROTTLE_POSITION_B, 0x33), /* 20 % */ \
        [PID_01_ABSOLUTE_THROTTLE_POSITION_C] = ECU_LIVE(PID_01_ABSOLUTE_THROTTLE_POSITION_C, 0x66), /* 40 % */ \
        [PID_01_ACCELERATOR_PEDAL_POSITION_D] = ECU_LIVE(PID_01_ACCELERATOR_PEDAL_POSITION_D, 0x1A), /* 10.196 % */ \
        [PID_01_ACCELERATOR_PEDAL_POSITION_E] = ECU_LIVE(PID_01_ACCELERATOR_PEDAL_POSITION_E, 0x0D), /* 5.098 % */ \
        [PID_01_ACCELERATOR_PEDAL_POSITION_F] = ECU_LIVE(PID_01_ACCELERATOR_PEDAL_POSITION_F, 0x00), /* 0 % */ \
        [PID_01_COMMANDED_THROTTLE_ACTUATOR]  = ECU_LIVE(PID_01_COMMANDED_THROTTLE_ACTUATOR, 0x99), /* 60 % */ \
        [PID_01_CONTROL_MODULE_VOLTAGE]       = ECU_LIVE(PID_01_CONTROL_MODULE_VOLTAGE, 0x36, 0xB0), /* 14 V */ \
        [PID_01_ABSOLUTE_LOAD_VALUE]          = ECU_LIVE(PID_01_ABSOLUTE_LOAD_VALUE, 0x00, 0x33), /* 20 % */ \
        [PID_01_RELATIVE_THROTTLE_POSITION]   = ECU_LIVE(PID_01_RELATIVE_THROTTLE_POSITION, 0x66), /* 40 % */ \
        [PID_01_AMBIENT_AIR_TEMP]             = ECU_LIVE(PID_01_AMBIENT_AIR_TEMP, 0x32),        /* 10 degC */ \
        [PID_01_TIME_WITH_MIL_ON]             = ECU_LIVE(PID_01_TIME_WITH_MIL_ON, 0x00, 0x3C),  /* 60 min */ \
        [PID_01_TIME_SINCE_CODES_CLEARED]     = ECU_LIVE(PID_01_TIME_SINCE_CODES_CLEARED, 0x01, 0x2C), /* 300 min */ \
        [PID_01_MAX_VALUES_FAER_O2V_O2C_MAP]  = ECU_LIVE(PID_01_MAX_VALUES_FAER_O2V_O2C_MAP, 0x04, 0x10, 0x00, 0x1E), /* 4, 16 V, -, 300 kPa */ \
        [PID_01_MAX_VALUE_MAF]                = ECU_LIVE(PID_01_MAX_VALUE_MAF, 0x32),           /* 500 g/s */ \
    }, \
    [SID_SHOW_FREEZE_FRAME_DATA] = \
    { \
        [PID_01_PIDS_SUPPORTED_01_20] = ECU_FREEZE(PID_01_PIDS_SUPPORTED_01_20, 0x7F, 0xBA, 0x00, 0x00), /* 02-09 0B-0D 0F */ \
        [PID_01_FREEZE_DTC]    = ECU_FREEZE(PID_01_FREEZE_DTC, 0x01, 0x43),   /* P0143 */ \
        [PID_01_COOLANT_TEMP]  = ECU_FREEZE(PID_01_COOLANT_TEMP, 0xBE),       /* 150 degC */ \
        [PID_01_ENGINE_RPM]    = ECU_FREEZE(PID_01_ENGINE_RPM, 0x2E, 0xE0),   /* 3000 rpm */ \
        [PID_01_VEHICLE_SPEED] = ECU_FREEZE(PID_01_VEHICLE_SPEED, 0x78),      /* 120 km/h */ \
        [PID_01_FUEL_SYSTEM_STATUS] = ECU_FREEZE(PID_01_FUEL_SYSTEM_STATUS, 0x02, 0x00), /* CL */ \
        [PID_01_ENGINE_LOAD]   = ECU_FREEZE(PID_01_ENGINE_LOAD, 0x99),        /* 60 % */ \
        [PID_01_SHORT_TERM_FUEL_TRIM_BANK1] = ECU_FREEZE(PID_01_SHORT_TERM_FUEL_TRIM_BANK1, 0x80), /* 0 % */ \
        [PID_01_LONG_TERM_FUEL_TRIM_BANK1]  = ECU_FREEZE(PID_01_LONG_TERM_FUEL_TRIM_BANK1, 0x85),  /* 3.906 % */ \
        [PID_01_SHORT_TERM_FUEL_TRIM_BANK2] = ECU_FREEZE(PID_01_SHORT_TERM_FUEL_TRIM_BANK2, 0x7B), /* -3.906 % */ \
        [PID_01_LONG_TERM_FUEL_TRIM_BANK2]  = ECU_FREEZE(PID_01_LONG_TERM_FUEL_TRIM_BANK2, 0x80),  /* 0 % */ \
        [PID_01_INTAKE_MAP]    = ECU_FREEZE(PID_01_INTAKE_MAP, 0x64),         /* 100 kPa */ \
        [PID_01_INTAKE_AIR_TEMP] = ECU_FREEZE(PID_01_INTAKE_AIR_TEMP, 0x41),  /* 25 degC */ \
    }, \
    [SID_SHOW_STORED_DTCS]  = { [0] = ECU_ANSWER_STORED_DTCS }, \
    [SID_TEST_RESULTS_O2_SENSORS] = \
    { \
        [0x00] = ECU_ANSWER(ECU_MSG(SID_TEST_RESULTS_O2_SENSORS, 0x00, 0x01, 0xE8, 0x00, 0x00, 0x00)), /* TIDs 1 2 3 5 */ \
        [0x01] = ECU_ANSWER(ECU_MSG(SID_TEST_RESULTS_O2_SENSORS, 0x01, 0x01, 0x5A)),             /* 0.45 V, constant */ \
        [0x05] = ECU_ANSWER(ECU_MSG(SID_TEST_RESULTS_O2_SENSORS, 0x05, 0x01, 0x19, 0x0A, 0x32)), /* 0.1 s in 0.04..0.2 */ \
    }, \
    [SID_TEST_RESULTS_OTHER] = \
    { \
        [0x00] = ECU_ANSWER(ECU_MSG(SID_TEST_RESULTS_OTHER, 0x00, 0xFF, 0x80, 0x00, 0x00, 0x00)), /* TID 1 */ \
        [0x01] = ECU_ANSWER( \
            ECU_MSG(SID_TEST_RESULTS_OTHER, 0x01, 0x01, 0x00, 0x64, 0x00, 0xC8),  /* comp 1: 100 <= max 200 */ \
            ECU_MSG(SID_TEST_RESULTS_OTHER, 0x01, 0x81, 0x00, 0x64, 0x00, 0x32),  /* comp 1: 100 >= min 50 */ \
            ECU_MSG(SID_TEST_RESULTS_OTHER, 0x01, 0x02, 0x01, 0x2C, 0x00, 0xC8)), /* comp 2: 300 > max 200, fails */ \
    }, \
    [SID_CONTROL_ONBOARD_SYSTEM] = \
    { \
        [0x00] = ECU_ANSWER(ECU_MSG(SID_CONTROL_ONBOARD_SYSTEM, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00)), /* TID 1 */ \
        [0x01] = ECU_ANSWER(ECU_MSG(SID_CONTROL_ONBOARD_SYSTEM, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00)), /* evap leak test */ \
    }, \
    [SID_CLEAR_DTCS]        = { [0] = ECU_ANSWER_CLEAR_DTCS }, \
    [SID_SHOW_PENDING_DTCS] = { [0] = ECU_ANSWER_PENDING_DTCS }, \
    [SID_PERMANENT_DTCS]    = { [0] = ECU_ANSWER_PERMANENT_DTCS }, \
    [SID_REQUEST_VEHICLE_INFO] = \
    { \
        [PID_09_INFOTYPE_SUPPORTED_01_20] = ECU_ANSWER(ECU_MSG(SID_REQUEST_VEHICLE_INFO, PID_09_INFOTYPE_SUPPORTED_01_20, 0x01, 0xFF, 0x00, 0x00, 0x00)), /* 01-08 */ \
        [PID_09_VIN_MESSAGE_COUNT]   = ECU_ANSWER_VIN_MESSAGE_COUNT, \
        [PID_09_VIN]                 = ECU_ANSWER_VIN, \
        [PID_09_CALID_MESSAGE_COUNT] = ECU_ANSWER_CALID_MESSAGE_COUNT, \
        [PID_09_CALID]               = ECU_ANSWER_CALID, \
        [PID_09_CVN_MESSAGE_COUNT]   = ECU_ANSWER_CVN_MESSAGE_COUNT, \
        [PID_09_CVN]                 = ECU_ANSWER_CVN, \
        [PID_09_IPTCOUNT_MSG_COUNT]  = ECU_ANSWER_IPT_MESSAGE_COUNT, \
        [PID_09_INUSE_PERF_TRACKING] = ECU_ANSWER_IPT, \
    }

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    libobd2_sreq_t serviceRequests; /* the messages the ECU sends back */
    size_t sreqLen;                 /* how many; 0 = no answer */
}ecu_libobd2_service_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* ECU_LIBOBD2_SERVICES_H */
