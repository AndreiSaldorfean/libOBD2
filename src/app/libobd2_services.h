#ifndef LIBOBD2_SERVICES_H
#define LIBOBD2_SERVICES_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include <stdint.h>
#include "libobd2_service_requests.h"
#include "libobd2_transfer_protocol.h"

/* ================================================= MACROS ================================================ */
#define OBD_SHORT_SID ((libobd2_sid_t)(0xA5U)) /* one request, one response */
#define OBD_LONG_SID  ((libobd2_sid_t)(0xC3)) /* runs a transfer protocol */

#define LIBOBD2_MAX_TP_STEPS  (32U) /* steps of one transfer protocol, TP_END included */

/* Table rows of the common service shapes */
#define OBD_LIVE_DATA_SERVICE(p)    { .sidSize = OBD_SHORT_SID, .serviceRequests = SREQ_PID(SID_SHOW_CURRENT_DATA, (p)) }
#define OBD_FREEZE_FRAME_SERVICE(p) { .sidSize = OBD_SHORT_SID, .serviceRequests = SREQ_FREEZE_FRAME(p) }
#define OBD_DTC_SERVICE(s)          { .sidSize = OBD_LONG_SID, .serviceRequests = SREQ_SID(s), .transferProtocol = TP_GET_DTCS }

#define OBD_SUPPORTED_SERVICES \
    /* $09 vehicle information */ \
    [OBD_GET_VIN]   = { .sidSize = OBD_LONG_SID, .serviceRequests = SREQ_GET_VIN,   .transferProtocol = TP_GET_VIN }, \
    [OBD_GET_CALID] = { .sidSize = OBD_LONG_SID, .serviceRequests = SREQ_GET_CALID, .transferProtocol = TP_GET_CALID }, \
    [OBD_GET_CVN]   = { .sidSize = OBD_LONG_SID, .serviceRequests = SREQ_GET_CVN,   .transferProtocol = TP_GET_CVN }, \
    [OBD_GET_IPT]   = { .sidSize = OBD_LONG_SID, .serviceRequests = SREQ_GET_IPT,   .transferProtocol = TP_GET_IPT }, \
    /* $01 current data */ \
    [OBD_GET_COOLANT_TEMP]          = OBD_LIVE_DATA_SERVICE(PID_01_COOLANT_TEMP), \
    [OBD_GET_PIDS_SUPPORTED_01_20]  = OBD_LIVE_DATA_SERVICE(PID_01_PIDS_SUPPORTED_01_20), \
    [OBD_GET_PIDS_SUPPORTED_21_40]  = OBD_LIVE_DATA_SERVICE(PID_01_PIDS_SUPPORTED_21_40), \
    [OBD_GET_PIDS_SUPPORTED_41_60]  = OBD_LIVE_DATA_SERVICE(PID_01_PIDS_SUPPORTED_41_60), \
    [OBD_GET_MONITOR_STATUS]        = OBD_LIVE_DATA_SERVICE(PID_01_MONITOR_STATUS), \
    [OBD_GET_FREEZE_DTC]            = OBD_LIVE_DATA_SERVICE(PID_01_FREEZE_DTC), \
    [OBD_GET_ENGINE_LOAD]           = OBD_LIVE_DATA_SERVICE(PID_01_ENGINE_LOAD), \
    [OBD_GET_SHORT_FUEL_TRIM_BANK1] = OBD_LIVE_DATA_SERVICE(PID_01_SHORT_TERM_FUEL_TRIM_BANK1), \
    [OBD_GET_LONG_FUEL_TRIM_BANK1]  = OBD_LIVE_DATA_SERVICE(PID_01_LONG_TERM_FUEL_TRIM_BANK1), \
    [OBD_GET_SHORT_FUEL_TRIM_BANK2] = OBD_LIVE_DATA_SERVICE(PID_01_SHORT_TERM_FUEL_TRIM_BANK2), \
    [OBD_GET_LONG_FUEL_TRIM_BANK2]  = OBD_LIVE_DATA_SERVICE(PID_01_LONG_TERM_FUEL_TRIM_BANK2), \
    [OBD_GET_FUEL_PRESSURE]         = OBD_LIVE_DATA_SERVICE(PID_01_FUEL_PRESSURE), \
    [OBD_GET_INTAKE_MAP]            = OBD_LIVE_DATA_SERVICE(PID_01_INTAKE_MAP), \
    [OBD_GET_ENGINE_RPM]            = OBD_LIVE_DATA_SERVICE(PID_01_ENGINE_RPM), \
    [OBD_GET_VEHICLE_SPEED]         = OBD_LIVE_DATA_SERVICE(PID_01_VEHICLE_SPEED), \
    [OBD_GET_TIMING_ADVANCE]        = OBD_LIVE_DATA_SERVICE(PID_01_TIMING_ADVANCE), \
    [OBD_GET_INTAKE_AIR_TEMP]       = OBD_LIVE_DATA_SERVICE(PID_01_INTAKE_AIR_TEMP), \
    [OBD_GET_MAF_FLOW_RATE]         = OBD_LIVE_DATA_SERVICE(PID_01_MAF_FLOW_RATE), \
    [OBD_GET_THROTTLE_POSITION]     = OBD_LIVE_DATA_SERVICE(PID_01_THROTTLE_POSITION), \
    [OBD_GET_RUN_TIME]              = OBD_LIVE_DATA_SERVICE(PID_01_RUN_TIME_ENGINE_START), \
    [OBD_GET_DISTANCE_WITH_MIL]     = OBD_LIVE_DATA_SERVICE(PID_01_DISTANCE_WITH_MIL), \
    [OBD_GET_FUEL_RAIL_PRESSURE]    = OBD_LIVE_DATA_SERVICE(PID_01_FUEL_RAIL_PRESSURE), \
    [OBD_GET_FUEL_RAIL_GAUGE_PRESSURE] = OBD_LIVE_DATA_SERVICE(PID_01_FUEL_RAIL_GAUGE_PRESSURE), \
    [OBD_GET_COMMANDED_EGR]         = OBD_LIVE_DATA_SERVICE(PID_01_COMMANDED_EGR), \
    [OBD_GET_EGR_ERROR]             = OBD_LIVE_DATA_SERVICE(PID_01_EGR_ERROR), \
    [OBD_GET_EVAP_PURGE]            = OBD_LIVE_DATA_SERVICE(PID_01_COMMANDED_EVAP_PURGE), \
    [OBD_GET_FUEL_LEVEL]            = OBD_LIVE_DATA_SERVICE(PID_01_FUEL_TANK_LEVEL), \
    [OBD_GET_WARMUPS_SINCE_CLEAR]   = OBD_LIVE_DATA_SERVICE(PID_01_WARMUPS_SINCE_CODES_CLEARED), \
    [OBD_GET_DISTANCE_SINCE_CLEAR]  = OBD_LIVE_DATA_SERVICE(PID_01_DISTANCE_SINCE_CODES_CLEARED), \
    [OBD_GET_EVAP_VAPOR_PRESSURE]   = OBD_LIVE_DATA_SERVICE(PID_01_EVAP_SYSTEM_VAPOR_PRESSURE), \
    [OBD_GET_BAROMETRIC_PRESSURE]   = OBD_LIVE_DATA_SERVICE(PID_01_ABSOLUTE_BAROMETRIC_PRESSURE), \
    [OBD_GET_CAT_TEMP_B1S1]         = OBD_LIVE_DATA_SERVICE(PID_01_CAT_TEMP_BANK1_SENSOR1), \
    [OBD_GET_CAT_TEMP_B2S1]         = OBD_LIVE_DATA_SERVICE(PID_01_CAT_TEMP_BANK2_SENSOR1), \
    [OBD_GET_CAT_TEMP_B1S2]         = OBD_LIVE_DATA_SERVICE(PID_01_CAT_TEMP_BANK1_SENSOR2), \
    [OBD_GET_CAT_TEMP_B2S2]         = OBD_LIVE_DATA_SERVICE(PID_01_CAT_TEMP_BANK2_SENSOR2), \
    [OBD_GET_MODULE_VOLTAGE]        = OBD_LIVE_DATA_SERVICE(PID_01_CONTROL_MODULE_VOLTAGE), \
    [OBD_GET_ABSOLUTE_LOAD]         = OBD_LIVE_DATA_SERVICE(PID_01_ABSOLUTE_LOAD_VALUE), \
    [OBD_GET_RELATIVE_THROTTLE]     = OBD_LIVE_DATA_SERVICE(PID_01_RELATIVE_THROTTLE_POSITION), \
    [OBD_GET_AMBIENT_AIR_TEMP]      = OBD_LIVE_DATA_SERVICE(PID_01_AMBIENT_AIR_TEMP), \
    [OBD_GET_TIME_WITH_MIL]         = OBD_LIVE_DATA_SERVICE(PID_01_TIME_WITH_MIL_ON), \
    [OBD_GET_TIME_SINCE_CLEAR]      = OBD_LIVE_DATA_SERVICE(PID_01_TIME_SINCE_CODES_CLEARED), \
    [OBD_GET_FUEL_SYSTEM_STATUS]    = OBD_LIVE_DATA_SERVICE(PID_01_FUEL_SYSTEM_STATUS), \
    [OBD_GET_SECONDARY_AIR_STATUS]  = OBD_LIVE_DATA_SERVICE(PID_01_SECONDARY_AIR_STATUS), \
    [OBD_GET_O2_SENSORS_PRESENT]    = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSORS_PRESENT), \
    [OBD_GET_O2_SENSORS_PRESENT_4BANKS] = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSORS_PRESENT_4BANKS), \
    [OBD_GET_OBD_STANDARD]          = OBD_LIVE_DATA_SERVICE(PID_01_OBD_STANDARDS), \
    [OBD_GET_PTO_STATUS]            = OBD_LIVE_DATA_SERVICE(PID_01_AUX_INPUT_STATUS), \
    [OBD_GET_MONITOR_STATUS_DRIVE_CYCLE] = OBD_LIVE_DATA_SERVICE(PID_01_MONITOR_STATUS_DRIVE_CYCLE), \
    [OBD_GET_COMMANDED_LAMBDA]      = OBD_LIVE_DATA_SERVICE(PID_01_FUEL_AIR_COMMANDED_EQUIV_RATIO), \
    [OBD_GET_THROTTLE_POSITION_B]   = OBD_LIVE_DATA_SERVICE(PID_01_ABSOLUTE_THROTTLE_POSITION_B), \
    [OBD_GET_THROTTLE_POSITION_C]   = OBD_LIVE_DATA_SERVICE(PID_01_ABSOLUTE_THROTTLE_POSITION_C), \
    [OBD_GET_PEDAL_POSITION_D]      = OBD_LIVE_DATA_SERVICE(PID_01_ACCELERATOR_PEDAL_POSITION_D), \
    [OBD_GET_PEDAL_POSITION_E]      = OBD_LIVE_DATA_SERVICE(PID_01_ACCELERATOR_PEDAL_POSITION_E), \
    [OBD_GET_PEDAL_POSITION_F]      = OBD_LIVE_DATA_SERVICE(PID_01_ACCELERATOR_PEDAL_POSITION_F), \
    [OBD_GET_THROTTLE_ACTUATOR]     = OBD_LIVE_DATA_SERVICE(PID_01_COMMANDED_THROTTLE_ACTUATOR), \
    [OBD_GET_O2_B1S1]               = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_1_BANK1), \
    [OBD_GET_O2_B1S2]               = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_2_BANK1), \
    [OBD_GET_O2_B1S3]               = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_3_BANK1), \
    [OBD_GET_O2_B1S4]               = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_4_BANK1), \
    [OBD_GET_O2_B2S1]               = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_1_BANK2), \
    [OBD_GET_O2_B2S2]               = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_2_BANK2), \
    [OBD_GET_O2_B2S3]               = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_3_BANK2), \
    [OBD_GET_O2_B2S4]               = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_4_BANK2), \
    [OBD_GET_WIDE_O2_V_B1S1]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_1_BANK1_FAER), \
    [OBD_GET_WIDE_O2_V_B1S2]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_2_BANK1_FAER), \
    [OBD_GET_WIDE_O2_V_B1S3]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_3_BANK1_FAER), \
    [OBD_GET_WIDE_O2_V_B1S4]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_4_BANK1_FAER), \
    [OBD_GET_WIDE_O2_V_B2S1]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_1_BANK2_FAER), \
    [OBD_GET_WIDE_O2_V_B2S2]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_2_BANK2_FAER), \
    [OBD_GET_WIDE_O2_V_B2S3]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_3_BANK2_FAER), \
    [OBD_GET_WIDE_O2_V_B2S4]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_4_BANK2_FAER), \
    [OBD_GET_WIDE_O2_I_B1S1]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_1_BANK1_FAER_CURRENT), \
    [OBD_GET_WIDE_O2_I_B1S2]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_2_BANK1_FAER_CURRENT), \
    [OBD_GET_WIDE_O2_I_B1S3]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_3_BANK1_FAER_CURRENT), \
    [OBD_GET_WIDE_O2_I_B1S4]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_4_BANK1_FAER_CURRENT), \
    [OBD_GET_WIDE_O2_I_B2S1]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_1_BANK2_FAER_CURRENT), \
    [OBD_GET_WIDE_O2_I_B2S2]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_2_BANK2_FAER_CURRENT), \
    [OBD_GET_WIDE_O2_I_B2S3]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_3_BANK2_FAER_CURRENT), \
    [OBD_GET_WIDE_O2_I_B2S4]        = OBD_LIVE_DATA_SERVICE(PID_01_O2_SENSOR_4_BANK2_FAER_CURRENT), \
    [OBD_GET_MAX_VALUES]            = OBD_LIVE_DATA_SERVICE(PID_01_MAX_VALUES_FAER_O2V_O2C_MAP), \
    [OBD_GET_MAX_MAF]               = OBD_LIVE_DATA_SERVICE(PID_01_MAX_VALUE_MAF), \
    /* $02 freeze frame (frame 0) */ \
    [OBD_GET_FF_DTC]                = OBD_FREEZE_FRAME_SERVICE(PID_01_FREEZE_DTC), \
    [OBD_GET_FF_COOLANT_TEMP]       = OBD_FREEZE_FRAME_SERVICE(PID_01_COOLANT_TEMP), \
    [OBD_GET_FF_ENGINE_RPM]         = OBD_FREEZE_FRAME_SERVICE(PID_01_ENGINE_RPM), \
    [OBD_GET_FF_VEHICLE_SPEED]      = OBD_FREEZE_FRAME_SERVICE(PID_01_VEHICLE_SPEED), \
    [OBD_GET_FF_FUEL_SYSTEM_STATUS] = OBD_FREEZE_FRAME_SERVICE(PID_01_FUEL_SYSTEM_STATUS), \
    [OBD_GET_FF_ENGINE_LOAD]        = OBD_FREEZE_FRAME_SERVICE(PID_01_ENGINE_LOAD), \
    [OBD_GET_FF_SHORT_FUEL_TRIM_BANK1] = OBD_FREEZE_FRAME_SERVICE(PID_01_SHORT_TERM_FUEL_TRIM_BANK1), \
    [OBD_GET_FF_LONG_FUEL_TRIM_BANK1]  = OBD_FREEZE_FRAME_SERVICE(PID_01_LONG_TERM_FUEL_TRIM_BANK1), \
    [OBD_GET_FF_SHORT_FUEL_TRIM_BANK2] = OBD_FREEZE_FRAME_SERVICE(PID_01_SHORT_TERM_FUEL_TRIM_BANK2), \
    [OBD_GET_FF_LONG_FUEL_TRIM_BANK2]  = OBD_FREEZE_FRAME_SERVICE(PID_01_LONG_TERM_FUEL_TRIM_BANK2), \
    [OBD_GET_FF_INTAKE_MAP]         = OBD_FREEZE_FRAME_SERVICE(PID_01_INTAKE_MAP), \
    [OBD_GET_FF_INTAKE_AIR_TEMP]    = OBD_FREEZE_FRAME_SERVICE(PID_01_INTAKE_AIR_TEMP), \
    /* $03 / $07 / $0A DTCs, $04 clear */ \
    [OBD_GET_STORED_DTCS]           = OBD_DTC_SERVICE(SID_SHOW_STORED_DTCS), \
    [OBD_GET_PENDING_DTCS]          = OBD_DTC_SERVICE(SID_SHOW_PENDING_DTCS), \
    [OBD_GET_PERMANENT_DTCS]        = OBD_DTC_SERVICE(SID_PERMANENT_DTCS), \
    [OBD_CLEAR_DTCS]                = { .sidSize = OBD_SHORT_SID, .serviceRequests = SREQ_SID(SID_CLEAR_DTCS) }

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t sidSize;
    libobd2_data_t serviceRequests[LIBOBD2_MAX_SREQ];
    uint8_t transferProtocol[LIBOBD2_MAX_TP_STEPS];
}libobd2_service_t;

/* The services LibOBD2_RequestService runs. The comment of each is what it writes into `response`
 * (host byte order, read it back with memcpy: the buffer may not be aligned for the type) */
typedef enum
{
    /* $09 vehicle information */
    OBD_GET_VIN,                      /* char[20]: 3 pad bytes $00 + 17 VIN characters */
    OBD_GET_CALID,                    /* char[16 * n]: n calibration IDs, $00 padded */
    OBD_GET_CVN,                      /* uint8_t[4 * n]: n CVNs, most significant byte first */
    OBD_GET_IPT,                      /* uint8_t[32]: 16 in-use performance counters, 2 bytes each, MSB first,
                                         in Table G.8 order (OBDCOND, IGNCYCCNTR, CATCOMP1, CATCOND1, ...) */

    /* $01 current data */
    OBD_GET_COOLANT_TEMP,             /* int16_t  degC */
    OBD_GET_PIDS_SUPPORTED_01_20,     /* uint32_t bit 31 = PID $01 ... bit 0 = PID $20 */
    OBD_GET_PIDS_SUPPORTED_21_40,     /* uint32_t bit 31 = PID $21 ... bit 0 = PID $40 */
    OBD_GET_PIDS_SUPPORTED_41_60,     /* uint32_t bit 31 = PID $41 ... bit 0 = PID $60 */
    OBD_GET_MONITOR_STATUS,           /* libobd2_monitor_status_t: MIL, DTC count, readiness (LibOBD2_IsReady) */
    OBD_GET_FREEZE_DTC,               /* uint16_t DTC (LibOBD2_DtcToString), 0 = none */
    OBD_GET_ENGINE_LOAD,              /* float    % */
    OBD_GET_SHORT_FUEL_TRIM_BANK1,    /* float    % */
    OBD_GET_LONG_FUEL_TRIM_BANK1,     /* float    % */
    OBD_GET_SHORT_FUEL_TRIM_BANK2,    /* float    % */
    OBD_GET_LONG_FUEL_TRIM_BANK2,     /* float    % */
    OBD_GET_FUEL_PRESSURE,            /* uint16_t kPa */
    OBD_GET_INTAKE_MAP,               /* uint8_t  kPa */
    OBD_GET_ENGINE_RPM,               /* float    rpm */
    OBD_GET_VEHICLE_SPEED,            /* uint8_t  km/h */
    OBD_GET_TIMING_ADVANCE,           /* float    degrees before TDC */
    OBD_GET_INTAKE_AIR_TEMP,          /* int16_t  degC */
    OBD_GET_MAF_FLOW_RATE,            /* float    g/s */
    OBD_GET_THROTTLE_POSITION,        /* float    % */
    OBD_GET_RUN_TIME,                 /* uint16_t s */
    OBD_GET_DISTANCE_WITH_MIL,        /* uint16_t km */
    OBD_GET_FUEL_RAIL_PRESSURE,       /* float    kPa */
    OBD_GET_FUEL_RAIL_GAUGE_PRESSURE, /* uint32_t kPa */
    OBD_GET_COMMANDED_EGR,            /* float    % */
    OBD_GET_EGR_ERROR,                /* float    % */
    OBD_GET_EVAP_PURGE,               /* float    % */
    OBD_GET_FUEL_LEVEL,               /* float    % */
    OBD_GET_WARMUPS_SINCE_CLEAR,      /* uint8_t  count */
    OBD_GET_DISTANCE_SINCE_CLEAR,     /* uint16_t km */
    OBD_GET_EVAP_VAPOR_PRESSURE,      /* float    Pa */
    OBD_GET_BAROMETRIC_PRESSURE,      /* uint8_t  kPa */
    OBD_GET_CAT_TEMP_B1S1,            /* float    degC */
    OBD_GET_CAT_TEMP_B2S1,            /* float    degC */
    OBD_GET_CAT_TEMP_B1S2,            /* float    degC */
    OBD_GET_CAT_TEMP_B2S2,            /* float    degC */
    OBD_GET_MODULE_VOLTAGE,           /* float    V */
    OBD_GET_ABSOLUTE_LOAD,            /* float    % */
    OBD_GET_RELATIVE_THROTTLE,        /* float    % */
    OBD_GET_AMBIENT_AIR_TEMP,         /* int16_t  degC */
    OBD_GET_TIME_WITH_MIL,            /* uint16_t min */
    OBD_GET_TIME_SINCE_CLEAR,         /* uint16_t min */
    OBD_GET_FUEL_SYSTEM_STATUS,       /* libobd2_fuel_system_status_t, LIBOBD2_FUELSYS_* bits */
    OBD_GET_SECONDARY_AIR_STATUS,     /* uint8_t  LIBOBD2_AIR_* bits */
    OBD_GET_O2_SENSORS_PRESENT,       /* uint8_t  bit n = bank n/4 + 1, sensor n%4 + 1 (PID $13) */
    OBD_GET_O2_SENSORS_PRESENT_4BANKS,/* uint8_t  bit n = bank n/2 + 1, sensor n%2 + 1 (PID $1D) */
    OBD_GET_OBD_STANDARD,             /* uint8_t  OBD requirements the vehicle is designed to (Table B.23) */
    OBD_GET_PTO_STATUS,               /* bool     power take-off active */
    OBD_GET_MONITOR_STATUS_DRIVE_CYCLE, /* libobd2_monitors_t, this drive cycle */
    OBD_GET_COMMANDED_LAMBDA,         /* float    commanded equivalence ratio */
    OBD_GET_THROTTLE_POSITION_B,      /* float    % */
    OBD_GET_THROTTLE_POSITION_C,      /* float    % */
    OBD_GET_PEDAL_POSITION_D,         /* float    % */
    OBD_GET_PEDAL_POSITION_E,         /* float    % */
    OBD_GET_PEDAL_POSITION_F,         /* float    % */
    OBD_GET_THROTTLE_ACTUATOR,        /* float    % */
    OBD_GET_O2_B1S1,                  /* libobd2_o2_sensor_t, PIDs $14-$1B: bank/sensor as with PID $13 */
    OBD_GET_O2_B1S2,                  /* libobd2_o2_sensor_t */
    OBD_GET_O2_B1S3,                  /* libobd2_o2_sensor_t */
    OBD_GET_O2_B1S4,                  /* libobd2_o2_sensor_t */
    OBD_GET_O2_B2S1,                  /* libobd2_o2_sensor_t */
    OBD_GET_O2_B2S2,                  /* libobd2_o2_sensor_t */
    OBD_GET_O2_B2S3,                  /* libobd2_o2_sensor_t */
    OBD_GET_O2_B2S4,                  /* libobd2_o2_sensor_t */
    OBD_GET_WIDE_O2_V_B1S1,           /* libobd2_o2_wide_voltage_t, PIDs $24-$2B */
    OBD_GET_WIDE_O2_V_B1S2,           /* libobd2_o2_wide_voltage_t */
    OBD_GET_WIDE_O2_V_B1S3,           /* libobd2_o2_wide_voltage_t */
    OBD_GET_WIDE_O2_V_B1S4,           /* libobd2_o2_wide_voltage_t */
    OBD_GET_WIDE_O2_V_B2S1,           /* libobd2_o2_wide_voltage_t */
    OBD_GET_WIDE_O2_V_B2S2,           /* libobd2_o2_wide_voltage_t */
    OBD_GET_WIDE_O2_V_B2S3,           /* libobd2_o2_wide_voltage_t */
    OBD_GET_WIDE_O2_V_B2S4,           /* libobd2_o2_wide_voltage_t */
    OBD_GET_WIDE_O2_I_B1S1,           /* libobd2_o2_wide_current_t, PIDs $34-$3B */
    OBD_GET_WIDE_O2_I_B1S2,           /* libobd2_o2_wide_current_t */
    OBD_GET_WIDE_O2_I_B1S3,           /* libobd2_o2_wide_current_t */
    OBD_GET_WIDE_O2_I_B1S4,           /* libobd2_o2_wide_current_t */
    OBD_GET_WIDE_O2_I_B2S1,           /* libobd2_o2_wide_current_t */
    OBD_GET_WIDE_O2_I_B2S2,           /* libobd2_o2_wide_current_t */
    OBD_GET_WIDE_O2_I_B2S3,           /* libobd2_o2_wide_current_t */
    OBD_GET_WIDE_O2_I_B2S4,           /* libobd2_o2_wide_current_t */
    OBD_GET_MAX_VALUES,               /* libobd2_max_values_t: full scales for LibOBD2_Rescale */
    OBD_GET_MAX_MAF,                  /* uint16_t g/s, full scale of PID $10 */

    /* $02 freeze frame 0, same types as the $01 PID */
    OBD_GET_FF_DTC,                   /* uint16_t DTC that stored the freeze frame, 0 = no freeze frame */
    OBD_GET_FF_COOLANT_TEMP,          /* int16_t  degC */
    OBD_GET_FF_ENGINE_RPM,            /* float    rpm */
    OBD_GET_FF_VEHICLE_SPEED,         /* uint8_t  km/h */
    OBD_GET_FF_FUEL_SYSTEM_STATUS,    /* libobd2_fuel_system_status_t */
    OBD_GET_FF_ENGINE_LOAD,           /* float    % */
    OBD_GET_FF_SHORT_FUEL_TRIM_BANK1, /* float    % */
    OBD_GET_FF_LONG_FUEL_TRIM_BANK1,  /* float    % */
    OBD_GET_FF_SHORT_FUEL_TRIM_BANK2, /* float    % */
    OBD_GET_FF_LONG_FUEL_TRIM_BANK2,  /* float    % */
    OBD_GET_FF_INTAKE_MAP,            /* uint8_t  kPa */
    OBD_GET_FF_INTAKE_AIR_TEMP,       /* int16_t  degC */

    /* DTCs */
    OBD_GET_STORED_DTCS,              /* uint16_t[n] ($03), responseLen = 2 * n, n = 0 when none */
    OBD_GET_PENDING_DTCS,             /* uint16_t[n] ($07) */
    OBD_GET_PERMANENT_DTCS,           /* uint16_t[n] ($0A) */
    OBD_CLEAR_DTCS,                   /* nothing, responseLen = 0 ($04) */

    OBD2_MAX_NUM_SERVICES
}libobd2_sid_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_SERVICES_H */
