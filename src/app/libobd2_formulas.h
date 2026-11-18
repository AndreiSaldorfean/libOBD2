#ifndef LIBOBD2_FORMULAS_H
#define LIBOBD2_FORMULAS_H

/*************************************************************************************************************
 * Scaling of the $01 / $02 PIDs (ISO 15031-5 Annex B).
 *
 * A formula takes the PID's data bytes (data[0] = A, data[1] = B, ...) and writes the decoded value into
 * `out` as the type listed next to the service in libobd2_sid_t, in host byte order. It returns the number
 * of bytes it wrote. The caller reads the value back with memcpy into a variable of that type.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "iso15031_5.h"
#include "libobd2_service_requests.h"
#include "utils.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define LIBOBD2_MULTIFRAME_DATA_IDX  (2U)  /* param[] index of the first data byte, after PID and message number */
#define LIBOBD2_TEMP_OFFSET          (40)  /* temperatures: A - 40 degC */
#define LIBOBD2_MIL_MASK             (0x80U) /* PID $01 A bit 7: MIL on */
#define LIBOBD2_DTC_CNT_MASK         (0x7FU) /* PID $01 A bits 0-6: number of confirmed DTCs */
#define LIBOBD2_DTC_STR_LEN          (6U)  /* "P0143" + NUL */
#define LIBOBD2_O2_TRIM_UNUSED       (0xFFU) /* PID $14-$1B B: sensor not used for the fuel trim */
#define LIBOBD2_PTO_MASK             (0x01U) /* PID $1E A bit 0: PTO active */

/* PID $03 bits, one per fuel system (libobd2_fuel_system_status_t), at most one set */
#define LIBOBD2_FUELSYS_OL           (0x01U) /* open loop, closed-loop conditions not met yet */
#define LIBOBD2_FUELSYS_CL           (0x02U) /* closed loop, using the O2 sensor(s) */
#define LIBOBD2_FUELSYS_OL_DRIVE     (0x04U) /* open loop due to driving conditions */
#define LIBOBD2_FUELSYS_OL_FAULT     (0x08U) /* open loop due to a detected fault */
#define LIBOBD2_FUELSYS_CL_FAULT     (0x10U) /* closed loop, fault with at least one O2 sensor */

/* Monitor bits of libobd2_monitors_t (PID $01 / $41 B-D) */
#define LIBOBD2_MON_MISFIRE          (0x01U) /* continuous */
#define LIBOBD2_MON_FUEL_SYSTEM      (0x02U) /* continuous */
#define LIBOBD2_MON_COMPONENTS       (0x04U) /* continuous: comprehensive component monitoring */
#define LIBOBD2_MON_CONTINUOUS_MASK  (0x07U)
#define LIBOBD2_MON_CATALYST         (0x01U) /* non-continuous */
#define LIBOBD2_MON_HEATED_CATALYST  (0x02U)
#define LIBOBD2_MON_EVAP             (0x04U)
#define LIBOBD2_MON_SECONDARY_AIR    (0x08U)
#define LIBOBD2_MON_AC_REFRIGERANT   (0x10U)
#define LIBOBD2_MON_O2_SENSOR        (0x20U)
#define LIBOBD2_MON_O2_HEATER        (0x40U)
#define LIBOBD2_MON_EGR              (0x80U)
#define LIBOBD2_MON_COMPLETION_SHIFT (4U)    /* B: support / enable bits 0-2, completion bits 4-6 */

/* Default full scale of the wide-range O2 / lambda PIDs, which PID $4F may replace */
#define LIBOBD2_DEFAULT_MAX_LAMBDA   (2.0f)   /* PIDs $24-$2B, $34-$3B, $44 */
#define LIBOBD2_DEFAULT_MAX_O2_V     (8.0f)   /* PIDs $24-$2B C D, V */
#define LIBOBD2_DEFAULT_MAX_O2_MA    (128.0f) /* PIDs $34-$3B C D, mA */

/* PID $12 bits, at most one set */
#define LIBOBD2_AIR_UPSTREAM         (0x01U) /* upstream of the first catalytic converter */
#define LIBOBD2_AIR_DOWNSTREAM       (0x02U) /* downstream of the first catalytic converter inlet */
#define LIBOBD2_AIR_OFF              (0x04U) /* atmosphere / off */

/* PID -> number of data bytes, formula. A PID missing here decodes as OBD_NOT_SUPPORTED. */
#define LIBOBD2_PID_FORMULAS \
    { PID_01_PIDS_SUPPORTED_01_20,         4U, LibOBD2_FormulaBitmask }, \
    { PID_01_MONITOR_STATUS,               4U, LibOBD2_FormulaMonitorStatus }, \
    { PID_01_FREEZE_DTC,                   2U, LibOBD2_FormulaDtc }, \
    { PID_01_FUEL_SYSTEM_STATUS,           2U, LibOBD2_FormulaFuelSystem }, \
    { PID_01_ENGINE_LOAD,                  1U, LibOBD2_FormulaPercent }, \
    { PID_01_COOLANT_TEMP,                 1U, LibOBD2_FormulaTemp }, \
    { PID_01_SHORT_TERM_FUEL_TRIM_BANK1,   1U, LibOBD2_FormulaSignedPercent }, \
    { PID_01_LONG_TERM_FUEL_TRIM_BANK1,    1U, LibOBD2_FormulaSignedPercent }, \
    { PID_01_SHORT_TERM_FUEL_TRIM_BANK2,   1U, LibOBD2_FormulaSignedPercent }, \
    { PID_01_LONG_TERM_FUEL_TRIM_BANK2,    1U, LibOBD2_FormulaSignedPercent }, \
    { PID_01_FUEL_PRESSURE,                1U, LibOBD2_FormulaFuelPressure }, \
    { PID_01_INTAKE_MAP,                   1U, LibOBD2_FormulaByte }, \
    { PID_01_ENGINE_RPM,                   2U, LibOBD2_FormulaRpm }, \
    { PID_01_VEHICLE_SPEED,                1U, LibOBD2_FormulaByte }, \
    { PID_01_TIMING_ADVANCE,               1U, LibOBD2_FormulaTimingAdvance }, \
    { PID_01_INTAKE_AIR_TEMP,              1U, LibOBD2_FormulaTemp }, \
    { PID_01_MAF_FLOW_RATE,                2U, LibOBD2_FormulaMaf }, \
    { PID_01_THROTTLE_POSITION,            1U, LibOBD2_FormulaPercent }, \
    { PID_01_SECONDARY_AIR_STATUS,         1U, LibOBD2_FormulaByte }, \
    { PID_01_O2_SENSORS_PRESENT,           1U, LibOBD2_FormulaByte }, \
    { PID_01_O2_SENSOR_1_BANK1,            2U, LibOBD2_FormulaO2Sensor }, \
    { PID_01_O2_SENSOR_2_BANK1,            2U, LibOBD2_FormulaO2Sensor }, \
    { PID_01_O2_SENSOR_3_BANK1,            2U, LibOBD2_FormulaO2Sensor }, \
    { PID_01_O2_SENSOR_4_BANK1,            2U, LibOBD2_FormulaO2Sensor }, \
    { PID_01_O2_SENSOR_1_BANK2,            2U, LibOBD2_FormulaO2Sensor }, \
    { PID_01_O2_SENSOR_2_BANK2,            2U, LibOBD2_FormulaO2Sensor }, \
    { PID_01_O2_SENSOR_3_BANK2,            2U, LibOBD2_FormulaO2Sensor }, \
    { PID_01_O2_SENSOR_4_BANK2,            2U, LibOBD2_FormulaO2Sensor }, \
    { PID_01_OBD_STANDARDS,                1U, LibOBD2_FormulaByte }, \
    { PID_01_O2_SENSORS_PRESENT_4BANKS,    1U, LibOBD2_FormulaByte }, \
    { PID_01_AUX_INPUT_STATUS,             1U, LibOBD2_FormulaPto }, \
    { PID_01_RUN_TIME_ENGINE_START,        2U, LibOBD2_FormulaWord }, \
    { PID_01_PIDS_SUPPORTED_21_40,         4U, LibOBD2_FormulaBitmask }, \
    { PID_01_DISTANCE_WITH_MIL,            2U, LibOBD2_FormulaWord }, \
    { PID_01_FUEL_RAIL_PRESSURE,           2U, LibOBD2_FormulaFuelRailPressure }, \
    { PID_01_FUEL_RAIL_GAUGE_PRESSURE,     2U, LibOBD2_FormulaFuelRailGauge }, \
    { PID_01_O2_SENSOR_1_BANK1_FAER,       4U, LibOBD2_FormulaO2WideVoltage }, \
    { PID_01_O2_SENSOR_2_BANK1_FAER,       4U, LibOBD2_FormulaO2WideVoltage }, \
    { PID_01_O2_SENSOR_3_BANK1_FAER,       4U, LibOBD2_FormulaO2WideVoltage }, \
    { PID_01_O2_SENSOR_4_BANK1_FAER,       4U, LibOBD2_FormulaO2WideVoltage }, \
    { PID_01_O2_SENSOR_1_BANK2_FAER,       4U, LibOBD2_FormulaO2WideVoltage }, \
    { PID_01_O2_SENSOR_2_BANK2_FAER,       4U, LibOBD2_FormulaO2WideVoltage }, \
    { PID_01_O2_SENSOR_3_BANK2_FAER,       4U, LibOBD2_FormulaO2WideVoltage }, \
    { PID_01_O2_SENSOR_4_BANK2_FAER,       4U, LibOBD2_FormulaO2WideVoltage }, \
    { PID_01_COMMANDED_EGR,                1U, LibOBD2_FormulaPercent }, \
    { PID_01_EGR_ERROR,                    1U, LibOBD2_FormulaSignedPercent }, \
    { PID_01_COMMANDED_EVAP_PURGE,         1U, LibOBD2_FormulaPercent }, \
    { PID_01_FUEL_TANK_LEVEL,              1U, LibOBD2_FormulaPercent }, \
    { PID_01_WARMUPS_SINCE_CODES_CLEARED,  1U, LibOBD2_FormulaByte }, \
    { PID_01_DISTANCE_SINCE_CODES_CLEARED, 2U, LibOBD2_FormulaWord }, \
    { PID_01_EVAP_SYSTEM_VAPOR_PRESSURE,   2U, LibOBD2_FormulaVaporPressure }, \
    { PID_01_ABSOLUTE_BAROMETRIC_PRESSURE, 1U, LibOBD2_FormulaByte }, \
    { PID_01_O2_SENSOR_1_BANK1_FAER_CURRENT, 4U, LibOBD2_FormulaO2WideCurrent }, \
    { PID_01_O2_SENSOR_2_BANK1_FAER_CURRENT, 4U, LibOBD2_FormulaO2WideCurrent }, \
    { PID_01_O2_SENSOR_3_BANK1_FAER_CURRENT, 4U, LibOBD2_FormulaO2WideCurrent }, \
    { PID_01_O2_SENSOR_4_BANK1_FAER_CURRENT, 4U, LibOBD2_FormulaO2WideCurrent }, \
    { PID_01_O2_SENSOR_1_BANK2_FAER_CURRENT, 4U, LibOBD2_FormulaO2WideCurrent }, \
    { PID_01_O2_SENSOR_2_BANK2_FAER_CURRENT, 4U, LibOBD2_FormulaO2WideCurrent }, \
    { PID_01_O2_SENSOR_3_BANK2_FAER_CURRENT, 4U, LibOBD2_FormulaO2WideCurrent }, \
    { PID_01_O2_SENSOR_4_BANK2_FAER_CURRENT, 4U, LibOBD2_FormulaO2WideCurrent }, \
    { PID_01_CAT_TEMP_BANK1_SENSOR1,       2U, LibOBD2_FormulaCatalystTemp }, \
    { PID_01_CAT_TEMP_BANK2_SENSOR1,       2U, LibOBD2_FormulaCatalystTemp }, \
    { PID_01_CAT_TEMP_BANK1_SENSOR2,       2U, LibOBD2_FormulaCatalystTemp }, \
    { PID_01_CAT_TEMP_BANK2_SENSOR2,       2U, LibOBD2_FormulaCatalystTemp }, \
    { PID_01_PIDS_SUPPORTED_41_60,         4U, LibOBD2_FormulaBitmask }, \
    { PID_01_MONITOR_STATUS_DRIVE_CYCLE,   4U, LibOBD2_FormulaDriveCycleMonitors }, \
    { PID_01_CONTROL_MODULE_VOLTAGE,       2U, LibOBD2_FormulaVoltage }, \
    { PID_01_ABSOLUTE_LOAD_VALUE,          2U, LibOBD2_FormulaAbsoluteLoad }, \
    { PID_01_FUEL_AIR_COMMANDED_EQUIV_RATIO, 2U, LibOBD2_FormulaEquivalenceRatio }, \
    { PID_01_RELATIVE_THROTTLE_POSITION,   1U, LibOBD2_FormulaPercent }, \
    { PID_01_AMBIENT_AIR_TEMP,             1U, LibOBD2_FormulaTemp }, \
    { PID_01_ABSOLUTE_THROTTLE_POSITION_B, 1U, LibOBD2_FormulaPercent }, \
    { PID_01_ABSOLUTE_THROTTLE_POSITION_C, 1U, LibOBD2_FormulaPercent }, \
    { PID_01_ACCELERATOR_PEDAL_POSITION_D, 1U, LibOBD2_FormulaPercent }, \
    { PID_01_ACCELERATOR_PEDAL_POSITION_E, 1U, LibOBD2_FormulaPercent }, \
    { PID_01_ACCELERATOR_PEDAL_POSITION_F, 1U, LibOBD2_FormulaPercent }, \
    { PID_01_COMMANDED_THROTTLE_ACTUATOR,  1U, LibOBD2_FormulaPercent }, \
    { PID_01_TIME_WITH_MIL_ON,             2U, LibOBD2_FormulaWord }, \
    { PID_01_TIME_SINCE_CODES_CLEARED,     2U, LibOBD2_FormulaWord }, \
    { PID_01_MAX_VALUES_FAER_O2V_O2C_MAP,  4U, LibOBD2_FormulaMaxValues }, \
    { PID_01_MAX_VALUE_MAF,                1U, LibOBD2_FormulaMaxMaf }

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* Readiness monitors, LIBOBD2_MON_* bits. For PID $01 (since DTCs cleared) 'supported' is whether the
 * vehicle has the monitor; for PID $41 (this drive cycle) it is whether the monitor is enabled. A bit set
 * in 'incomplete' is a monitor that has not completed (not ready). */
typedef struct
{
    uint8_t continuousSupported;     /* B bits 0-2 */
    uint8_t continuousIncomplete;    /* B bits 4-6 */
    uint8_t nonContinuousSupported;  /* C */
    uint8_t nonContinuousIncomplete; /* D */
} libobd2_monitors_t;

/* Decoded PID $01 (monitor status since DTCs cleared) */
typedef struct
{
    bool milOn;                 /* A bit 7: malfunction indicator lamp commanded on */
    uint8_t dtcCount;           /* A bits 0-6: confirmed emission-related DTCs */
    libobd2_monitors_t monitors; /* B, C, D: readiness */
} libobd2_monitor_status_t;

/* Decoded PID $4F: full scales replacing the defaults (0 = keep the default), see LibOBD2_Rescale */
typedef struct
{
    uint8_t maxLambda;    /* A: PIDs $24-$2B, $34-$3B, $44 */
    uint8_t maxO2Voltage; /* B: V, PIDs $24-$2B */
    uint8_t maxO2Current; /* C: mA, PIDs $34-$3B */
    uint16_t maxMap;      /* D * 10 kPa, PID $0B */
} libobd2_max_values_t;

/* Decoded PID $03, one LIBOBD2_FUELSYS_* bit (or none) per fuel system */
typedef struct
{
    uint8_t fuelSystem1; /* A */
    uint8_t fuelSystem2; /* B, 0 when the vehicle has one fuel system */
} libobd2_fuel_system_status_t;

/* Decoded PID $14-$1B: conventional 0-1 V oxygen sensor */
typedef struct
{
    float voltage;       /* V, A * 0.005 */
    float shortTermTrim; /* %, (B - 128) * 100 / 128; meaningful only when trimUsed */
    bool trimUsed;       /* false when B = $FF: the sensor is not used for the fuel trim */
} libobd2_o2_sensor_t;

/* Decoded PID $24-$2B: wide-range oxygen sensor, equivalence ratio and voltage */
typedef struct
{
    float lambda;  /* (256A + B) * 2 / 65535 */
    float voltage; /* V, (256C + D) * 8 / 65535 */
} libobd2_o2_wide_voltage_t;

/* Decoded PID $34-$3B: wide-range oxygen sensor, equivalence ratio and current */
typedef struct
{
    float lambda;  /* (256A + B) * 2 / 65535 */
    float current; /* mA, (256C + D) / 256 - 128 */
} libobd2_o2_wide_current_t;

typedef size_t (*libobd2_formula_t)(const uint8_t *data, uint8_t *out);

typedef struct
{
    uint8_t pid;
    uint8_t dataLen;          /* data bytes A.. the formula reads */
    libobd2_formula_t formula;
} libobd2_pid_formula_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* 256 * A + B */
static inline uint16_t LibOBD2_Word(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

/* Copies one decoded value into out, returns its size */
static inline size_t LibOBD2_Put(uint8_t *out, const void *value, size_t size)
{
    memcpy(out, value, size);
    return size;
}

/******************************************
* title: LibOBD2_FormulaMultiFrame
* brief: Joins the data bytes of a $09 multi-message answer (VIN, CALID, ...)
*        into out and returns how many bytes it wrote.
******************************************/
static inline size_t LibOBD2_FormulaMultiFrame(const libobd2_data_t *msgs, size_t msgCnt, uint8_t *out)
{
    size_t outLen   = 0;
    size_t paramCnt = 0;

    OBD2_DEBUG_ASSERT((NULL != msgs) && (NULL != out));

    for (size_t msg = 0; msg < msgCnt; msg++)
    {
        /* dataLen counts the sid, the rest are params */
        paramCnt = (0U < msgs[msg].dataLen) ? (msgs[msg].dataLen - 1U) : 0U;

        for (size_t param = LIBOBD2_MULTIFRAME_DATA_IDX; param < paramCnt; param++)
        {
            out[outLen++] = msgs[msg].data.param[param];
        }
    }

    return outLen;
}

/* uint8_t: A (speed km/h, MAP/baro kPa, warm-ups) */
static inline size_t LibOBD2_FormulaByte(const uint8_t *data, uint8_t *out)
{
    return LibOBD2_Put(out, &data[0], sizeof(uint8_t));
}

/* uint16_t: 256A + B (run time s, distances km, times min) */
static inline size_t LibOBD2_FormulaWord(const uint8_t *data, uint8_t *out)
{
    const uint16_t value = LibOBD2_Word(data);

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* uint32_t: A B C D, bit 31 (A bit 7) = first PID of the range */
static inline size_t LibOBD2_FormulaBitmask(const uint8_t *data, uint8_t *out)
{
    const uint32_t value = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* libobd2_monitors_t from data bytes B, C, D */
static inline libobd2_monitors_t LibOBD2_Monitors(const uint8_t *bcd)
{
    const libobd2_monitors_t value =
    {
        .continuousSupported     = (uint8_t)(bcd[0] & LIBOBD2_MON_CONTINUOUS_MASK),
        .continuousIncomplete    = (uint8_t)((bcd[0] >> LIBOBD2_MON_COMPLETION_SHIFT) & LIBOBD2_MON_CONTINUOUS_MASK),
        .nonContinuousSupported  = bcd[1],
        .nonContinuousIncomplete = bcd[2],
    };

    return value;
}

/* libobd2_monitor_status_t: MIL = A bit 7, DTC count = A bits 0-6, readiness = B C D */
static inline size_t LibOBD2_FormulaMonitorStatus(const uint8_t *data, uint8_t *out)
{
    const libobd2_monitor_status_t value =
    {
        .milOn    = (0U != (data[0] & LIBOBD2_MIL_MASK)),
        .dtcCount = (uint8_t)(data[0] & LIBOBD2_DTC_CNT_MASK),
        .monitors = LibOBD2_Monitors(&data[1]),
    };

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* libobd2_monitors_t: PID $41, A is reserved */
static inline size_t LibOBD2_FormulaDriveCycleMonitors(const uint8_t *data, uint8_t *out)
{
    const libobd2_monitors_t value = LibOBD2_Monitors(&data[1]);

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* libobd2_max_values_t: A, B, C, D * 10 kPa */
static inline size_t LibOBD2_FormulaMaxValues(const uint8_t *data, uint8_t *out)
{
    const libobd2_max_values_t value =
    {
        .maxLambda    = data[0],
        .maxO2Voltage = data[1],
        .maxO2Current = data[2],
        .maxMap       = (uint16_t)(data[3] * 10U),
    };

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* uint16_t: A * 10 g/s (PID $50, max MAF) */
static inline size_t LibOBD2_FormulaMaxMaf(const uint8_t *data, uint8_t *out)
{
    const uint16_t value = (uint16_t)(data[0] * 10U);

    return LibOBD2_Put(out, &value, sizeof(value));
}

/******************************************
* title: LibOBD2_IsReady
* brief: True when every supported (enabled) monitor has completed: what an
*        I/M readiness check looks at.
******************************************/
static inline bool LibOBD2_IsReady(const libobd2_monitors_t *monitors)
{
    OBD2_DEBUG_ASSERT(NULL != monitors);

    return (0U == (monitors->continuousSupported & monitors->continuousIncomplete)) &&
           (0U == (monitors->nonContinuousSupported & monitors->nonContinuousIncomplete));
}

/******************************************
* title: LibOBD2_Rescale
* brief: Rescales a value decoded with the default full scale (one of
*        LIBOBD2_DEFAULT_MAX_*) to the full scale PID $4F reports.
*        newMax = 0 keeps the default.
******************************************/
static inline float LibOBD2_Rescale(float value, float defaultMax, uint8_t newMax)
{
    return (0U == newMax) ? value : (value * (float)newMax / defaultMax);
}

/* uint16_t: the 2 DTC bytes as one value, A = high byte */
static inline size_t LibOBD2_FormulaDtc(const uint8_t *data, uint8_t *out)
{
    return LibOBD2_FormulaWord(data, out);
}

/* int16_t: A - 40 degC (coolant, intake air, ambient air) */
static inline size_t LibOBD2_FormulaTemp(const uint8_t *data, uint8_t *out)
{
    const int16_t value = (int16_t)((int16_t)data[0] - LIBOBD2_TEMP_OFFSET);

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: A * 100 / 255 % (load, throttle, EGR, purge, fuel level) */
static inline size_t LibOBD2_FormulaPercent(const uint8_t *data, uint8_t *out)
{
    const float value = (float)data[0] * 100.0f / 255.0f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: (A - 128) * 100 / 128 % (fuel trims, EGR error) */
static inline size_t LibOBD2_FormulaSignedPercent(const uint8_t *data, uint8_t *out)
{
    const float value = ((float)data[0] - 128.0f) * 100.0f / 128.0f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* uint16_t: 3A kPa */
static inline size_t LibOBD2_FormulaFuelPressure(const uint8_t *data, uint8_t *out)
{
    const uint16_t value = (uint16_t)(3U * data[0]);

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: (256A + B) / 4 rpm */
static inline size_t LibOBD2_FormulaRpm(const uint8_t *data, uint8_t *out)
{
    const float value = (float)LibOBD2_Word(data) / 4.0f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: A / 2 - 64 degrees */
static inline size_t LibOBD2_FormulaTimingAdvance(const uint8_t *data, uint8_t *out)
{
    const float value = ((float)data[0] / 2.0f) - 64.0f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: (256A + B) / 100 g/s */
static inline size_t LibOBD2_FormulaMaf(const uint8_t *data, uint8_t *out)
{
    const float value = (float)LibOBD2_Word(data) / 100.0f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: (256A + B) * 0.079 kPa */
static inline size_t LibOBD2_FormulaFuelRailPressure(const uint8_t *data, uint8_t *out)
{
    const float value = (float)LibOBD2_Word(data) * 0.079f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* uint32_t: (256A + B) * 10 kPa */
static inline size_t LibOBD2_FormulaFuelRailGauge(const uint8_t *data, uint8_t *out)
{
    const uint32_t value = (uint32_t)LibOBD2_Word(data) * 10U;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: signed (256A + B) * 0.25 Pa */
static inline size_t LibOBD2_FormulaVaporPressure(const uint8_t *data, uint8_t *out)
{
    const float value = (float)(int16_t)LibOBD2_Word(data) * 0.25f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: (256A + B) / 10 - 40 degC */
static inline size_t LibOBD2_FormulaCatalystTemp(const uint8_t *data, uint8_t *out)
{
    const float value = ((float)LibOBD2_Word(data) / 10.0f) - 40.0f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: (256A + B) / 1000 V */
static inline size_t LibOBD2_FormulaVoltage(const uint8_t *data, uint8_t *out)
{
    const float value = (float)LibOBD2_Word(data) / 1000.0f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: (256A + B) * 100 / 255 % */
static inline size_t LibOBD2_FormulaAbsoluteLoad(const uint8_t *data, uint8_t *out)
{
    const float value = (float)LibOBD2_Word(data) * 100.0f / 255.0f;

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* libobd2_fuel_system_status_t: A, B */
static inline size_t LibOBD2_FormulaFuelSystem(const uint8_t *data, uint8_t *out)
{
    const libobd2_fuel_system_status_t value = { .fuelSystem1 = data[0], .fuelSystem2 = data[1] };

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* bool: PTO active, A bit 0 */
static inline size_t LibOBD2_FormulaPto(const uint8_t *data, uint8_t *out)
{
    const bool value = (0U != (data[0] & LIBOBD2_PTO_MASK));

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* (256A + B) * 2 / 65535 */
static inline float LibOBD2_Lambda(const uint8_t *data)
{
    return (float)LibOBD2_Word(data) * 2.0f / 65535.0f;
}

/* libobd2_o2_sensor_t: A * 0.005 V, (B - 128) * 100 / 128 % */
static inline size_t LibOBD2_FormulaO2Sensor(const uint8_t *data, uint8_t *out)
{
    const libobd2_o2_sensor_t value =
    {
        .voltage       = (float)data[0] * 0.005f,
        .shortTermTrim = ((float)data[1] - 128.0f) * 100.0f / 128.0f,
        .trimUsed      = (LIBOBD2_O2_TRIM_UNUSED != data[1]),
    };

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* libobd2_o2_wide_voltage_t: lambda from A B, (256C + D) * 8 / 65535 V */
static inline size_t LibOBD2_FormulaO2WideVoltage(const uint8_t *data, uint8_t *out)
{
    const libobd2_o2_wide_voltage_t value =
    {
        .lambda  = LibOBD2_Lambda(&data[0]),
        .voltage = (float)LibOBD2_Word(&data[2]) * 8.0f / 65535.0f,
    };

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* libobd2_o2_wide_current_t: lambda from A B, (256C + D) / 256 - 128 mA */
static inline size_t LibOBD2_FormulaO2WideCurrent(const uint8_t *data, uint8_t *out)
{
    const libobd2_o2_wide_current_t value =
    {
        .lambda  = LibOBD2_Lambda(&data[0]),
        .current = ((float)LibOBD2_Word(&data[2]) / 256.0f) - 128.0f,
    };

    return LibOBD2_Put(out, &value, sizeof(value));
}

/* float: commanded equivalence ratio (lambda), (256A + B) * 2 / 65535 */
static inline size_t LibOBD2_FormulaEquivalenceRatio(const uint8_t *data, uint8_t *out)
{
    const float value = LibOBD2_Lambda(data);

    return LibOBD2_Put(out, &value, sizeof(value));
}

/******************************************
* title: LibOBD2_DtcToString
* brief: Writes a DTC as displayed (ISO 15031-5 6.3.1): $0143 -> "P0143".
*        str holds at least LIBOBD2_DTC_STR_LEN chars.
******************************************/
static inline void LibOBD2_DtcToString(uint16_t dtc, char *str)
{
    static const char system[] = { 'P', 'C', 'B', 'U' };
    static const char hex[]    = "0123456789ABCDEF";

    OBD2_DEBUG_ASSERT(NULL != str);

    str[0] = system[(dtc >> 14) & 0x3U];
    str[1] = hex[(dtc >> 12) & 0x3U];
    str[2] = hex[(dtc >> 8) & 0xFU];
    str[3] = hex[(dtc >> 4) & 0xFU];
    str[4] = hex[dtc & 0xFU];
    str[5] = '\0';
}

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_FORMULAS_H */
