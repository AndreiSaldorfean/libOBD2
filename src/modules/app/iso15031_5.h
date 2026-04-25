/**
 * @file iso15031_5.h
 * @brief ISO 15031-5 / SAE J1979 OBD-II Service and PID definitions
 *
 * This file contains the service identifiers (SIDs) and parameter identifiers (PIDs)
 * as defined in ISO 15031-5 (Road vehicles - Communication between vehicle and
 * external equipment for emissions-related diagnostics).
 */

#ifndef ISO15031_5_H
#define ISO15031_5_H

#include <stdint.h>

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
#define SID_SHIFT                           (0x9U)
#define PID_SHIFT                           (0x0U)
#define SID_MASK                            (0xFE00U)
#define PID_MASK                            (0x01FFU)
#define GET_HASH(SID, PID)                  (((SID << SID_SHIFT) & SID_MASK) | ((PID << PID_SHIFT) & PID_MASK))
#define GET_SID(hash)                       ((hash & SID_MASK) >> SID_SHIFT)
#define GET_PID(hash)                       ((hash & PID_MASK) >> PID_SHIFT)

/**
 * @brief Service Identifiers (SIDs) - Also known as Modes
 */
#define SID_SHOW_CURRENT_DATA               0x01  /* Service $01: Show Current Data */
#define SID_SHOW_FREEZE_FRAME_DATA          0x02  /* Service $02: Show Freeze Frame Data */
#define SID_SHOW_STORED_DTCS                0x03  /* Service $03: Show Stored Diagnostic Trouble Codes */
#define SID_CLEAR_DTCS                      0x04  /* Service $04: Clear Diagnostic Trouble Codes and Stored Values */
#define SID_TEST_RESULTS_O2_SENSORS         0x05  /* Service $05: Test Results, Oxygen Sensor Monitoring (non-CAN only) */
#define SID_TEST_RESULTS_OTHER              0x06  /* Service $06: Test Results, Other Component/System Monitoring */
#define SID_SHOW_PENDING_DTCS               0x07  /* Service $07: Show Pending Diagnostic Trouble Codes */
#define SID_CONTROL_ONBOARD_SYSTEM          0x08  /* Service $08: Control Operation of On-board Component/System */
#define SID_REQUEST_VEHICLE_INFO            0x09  /* Service $09: Request Vehicle Information */
#define SID_PERMANENT_DTCS                  0x0A  /* Service $0A: Permanent DTCs (Cleared DTCs) */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */

/**
 * @brief Service $01 - Show Current Data PIDs
 */
typedef enum {
    /* PIDs supported [01-20] */
    PID_01_PIDS_SUPPORTED_01_20             = 0x00, /* PIDs supported [01-20] */
    PID_01_MONITOR_STATUS                   = 0x01, /* Monitor status since DTCs cleared */
    PID_01_FREEZE_DTC                       = 0x02, /* Freeze DTC */
    PID_01_FUEL_SYSTEM_STATUS               = 0x03, /* Fuel system status */
    PID_01_ENGINE_LOAD                      = 0x04, /* Calculated engine load */
    PID_01_COOLANT_TEMP                     = 0x05, /* Engine coolant temperature */
    PID_01_SHORT_TERM_FUEL_TRIM_BANK1       = 0x06, /* Short term fuel trim - Bank 1 */
    PID_01_LONG_TERM_FUEL_TRIM_BANK1        = 0x07, /* Long term fuel trim - Bank 1 */
    PID_01_SHORT_TERM_FUEL_TRIM_BANK2       = 0x08, /* Short term fuel trim - Bank 2 */
    PID_01_LONG_TERM_FUEL_TRIM_BANK2        = 0x09, /* Long term fuel trim - Bank 2 */
    PID_01_FUEL_PRESSURE                    = 0x0A, /* Fuel pressure (gauge pressure) */
    PID_01_INTAKE_MAP                       = 0x0B, /* Intake manifold absolute pressure */
    PID_01_ENGINE_RPM                       = 0x0C, /* Engine RPM */
    PID_01_VEHICLE_SPEED                    = 0x0D, /* Vehicle speed */
    PID_01_TIMING_ADVANCE                   = 0x0E, /* Timing advance */
    PID_01_INTAKE_AIR_TEMP                  = 0x0F, /* Intake air temperature */
    PID_01_MAF_FLOW_RATE                    = 0x10, /* Mass air flow sensor (MAF) air flow rate */
    PID_01_THROTTLE_POSITION                = 0x11, /* Throttle position */
    PID_01_SECONDARY_AIR_STATUS             = 0x12, /* Commanded secondary air status */
    PID_01_O2_SENSORS_PRESENT               = 0x13, /* Oxygen sensors present (in 2 banks) */
    PID_01_O2_SENSOR_1_BANK1                = 0x14, /* Oxygen Sensor 1, Bank 1 */
    PID_01_O2_SENSOR_2_BANK1                = 0x15, /* Oxygen Sensor 2, Bank 1 */
    PID_01_O2_SENSOR_3_BANK1                = 0x16, /* Oxygen Sensor 3, Bank 1 */
    PID_01_O2_SENSOR_4_BANK1                = 0x17, /* Oxygen Sensor 4, Bank 1 */
    PID_01_O2_SENSOR_1_BANK2                = 0x18, /* Oxygen Sensor 1, Bank 2 */
    PID_01_O2_SENSOR_2_BANK2                = 0x19, /* Oxygen Sensor 2, Bank 2 */
    PID_01_O2_SENSOR_3_BANK2                = 0x1A, /* Oxygen Sensor 3, Bank 2 */
    PID_01_O2_SENSOR_4_BANK2                = 0x1B, /* Oxygen Sensor 4, Bank 2 */
    PID_01_OBD_STANDARDS                    = 0x1C, /* OBD standards this vehicle conforms to */
    PID_01_O2_SENSORS_PRESENT_4BANKS        = 0x1D, /* Oxygen sensors present (in 4 banks) */
    PID_01_AUX_INPUT_STATUS                 = 0x1E, /* Auxiliary input status */
    PID_01_RUN_TIME_ENGINE_START            = 0x1F, /* Run time since engine start */

    /* PIDs supported [21-40] */
    PID_01_PIDS_SUPPORTED_21_40             = 0x20, /* PIDs supported [21-40] */
    PID_01_DISTANCE_WITH_MIL                = 0x21, /* Distance traveled with malfunction indicator lamp (MIL) on */
    PID_01_FUEL_RAIL_PRESSURE               = 0x22, /* Fuel rail pressure (relative to manifold vacuum) */
    PID_01_FUEL_RAIL_GAUGE_PRESSURE         = 0x23, /* Fuel rail gauge pressure (diesel, or gasoline direct injection) */
    PID_01_O2_SENSOR_1_BANK1_FAER           = 0x24, /* O2 Sensor 1, Bank 1 - Fuel-Air Equivalence Ratio */
    PID_01_O2_SENSOR_2_BANK1_FAER           = 0x25, /* O2 Sensor 2, Bank 1 - Fuel-Air Equivalence Ratio */
    PID_01_O2_SENSOR_3_BANK1_FAER           = 0x26, /* O2 Sensor 3, Bank 1 - Fuel-Air Equivalence Ratio */
    PID_01_O2_SENSOR_4_BANK1_FAER           = 0x27, /* O2 Sensor 4, Bank 1 - Fuel-Air Equivalence Ratio */
    PID_01_O2_SENSOR_1_BANK2_FAER           = 0x28, /* O2 Sensor 1, Bank 2 - Fuel-Air Equivalence Ratio */
    PID_01_O2_SENSOR_2_BANK2_FAER           = 0x29, /* O2 Sensor 2, Bank 2 - Fuel-Air Equivalence Ratio */
    PID_01_O2_SENSOR_3_BANK2_FAER           = 0x2A, /* O2 Sensor 3, Bank 2 - Fuel-Air Equivalence Ratio */
    PID_01_O2_SENSOR_4_BANK2_FAER           = 0x2B, /* O2 Sensor 4, Bank 2 - Fuel-Air Equivalence Ratio */
    PID_01_COMMANDED_EGR                    = 0x2C, /* Commanded EGR */
    PID_01_EGR_ERROR                        = 0x2D, /* EGR Error */
    PID_01_COMMANDED_EVAP_PURGE             = 0x2E, /* Commanded evaporative purge */
    PID_01_FUEL_TANK_LEVEL                  = 0x2F, /* Fuel tank level input */
    PID_01_WARMUPS_SINCE_CODES_CLEARED      = 0x30, /* Warm-ups since codes cleared */
    PID_01_DISTANCE_SINCE_CODES_CLEARED     = 0x31, /* Distance traveled since codes cleared */
    PID_01_EVAP_SYSTEM_VAPOR_PRESSURE       = 0x32, /* Evap. System Vapor Pressure */
    PID_01_ABSOLUTE_BAROMETRIC_PRESSURE     = 0x33, /* Absolute Barometric Pressure */
    PID_01_O2_SENSOR_1_BANK1_FAER_CURRENT   = 0x34, /* O2 Sensor 1, Bank 1 - Fuel-Air Equiv Ratio, Current */
    PID_01_O2_SENSOR_2_BANK1_FAER_CURRENT   = 0x35, /* O2 Sensor 2, Bank 1 - Fuel-Air Equiv Ratio, Current */
    PID_01_O2_SENSOR_3_BANK1_FAER_CURRENT   = 0x36, /* O2 Sensor 3, Bank 1 - Fuel-Air Equiv Ratio, Current */
    PID_01_O2_SENSOR_4_BANK1_FAER_CURRENT   = 0x37, /* O2 Sensor 4, Bank 1 - Fuel-Air Equiv Ratio, Current */
    PID_01_O2_SENSOR_1_BANK2_FAER_CURRENT   = 0x38, /* O2 Sensor 1, Bank 2 - Fuel-Air Equiv Ratio, Current */
    PID_01_O2_SENSOR_2_BANK2_FAER_CURRENT   = 0x39, /* O2 Sensor 2, Bank 2 - Fuel-Air Equiv Ratio, Current */
    PID_01_O2_SENSOR_3_BANK2_FAER_CURRENT   = 0x3A, /* O2 Sensor 3, Bank 2 - Fuel-Air Equiv Ratio, Current */
    PID_01_O2_SENSOR_4_BANK2_FAER_CURRENT   = 0x3B, /* O2 Sensor 4, Bank 2 - Fuel-Air Equiv Ratio, Current */
    PID_01_CAT_TEMP_BANK1_SENSOR1           = 0x3C, /* Catalyst Temperature: Bank 1, Sensor 1 */
    PID_01_CAT_TEMP_BANK2_SENSOR1           = 0x3D, /* Catalyst Temperature: Bank 2, Sensor 1 */
    PID_01_CAT_TEMP_BANK1_SENSOR2           = 0x3E, /* Catalyst Temperature: Bank 1, Sensor 2 */
    PID_01_CAT_TEMP_BANK2_SENSOR2           = 0x3F, /* Catalyst Temperature: Bank 2, Sensor 2 */

    /* PIDs supported [41-60] */
    PID_01_PIDS_SUPPORTED_41_60             = 0x40, /* PIDs supported [41-60] */
    PID_01_MONITOR_STATUS_DRIVE_CYCLE       = 0x41, /* Monitor status this drive cycle */
    PID_01_CONTROL_MODULE_VOLTAGE           = 0x42, /* Control module voltage */
    PID_01_ABSOLUTE_LOAD_VALUE              = 0x43, /* Absolute load value */
    PID_01_FUEL_AIR_COMMANDED_EQUIV_RATIO   = 0x44, /* Fuel–Air commanded equivalence ratio */
    PID_01_RELATIVE_THROTTLE_POSITION       = 0x45, /* Relative throttle position */
    PID_01_AMBIENT_AIR_TEMP                 = 0x46, /* Ambient air temperature */
    PID_01_ABSOLUTE_THROTTLE_POSITION_B     = 0x47, /* Absolute throttle position B */
    PID_01_ABSOLUTE_THROTTLE_POSITION_C     = 0x48, /* Absolute throttle position C */
    PID_01_ACCELERATOR_PEDAL_POSITION_D     = 0x49, /* Accelerator pedal position D */
    PID_01_ACCELERATOR_PEDAL_POSITION_E     = 0x4A, /* Accelerator pedal position E */
    PID_01_ACCELERATOR_PEDAL_POSITION_F     = 0x4B, /* Accelerator pedal position F */
    PID_01_COMMANDED_THROTTLE_ACTUATOR      = 0x4C, /* Commanded throttle actuator */
    PID_01_TIME_WITH_MIL_ON                 = 0x4D, /* Time run with MIL on */
    PID_01_TIME_SINCE_CODES_CLEARED         = 0x4E, /* Time since trouble codes cleared */
    PID_01_MAX_VALUES_FAER_O2V_O2C_MAP      = 0x4F, /* Maximum value for Fuel–Air equiv ratio, O2 sensor voltage, O2 sensor current, intake manifold absolute pressure */
    PID_01_MAX_VALUE_MAF                    = 0x50, /* Maximum value for air flow rate from mass air flow sensor */
    PID_01_FUEL_TYPE                        = 0x51, /* Fuel Type */
    PID_01_ETHANOL_FUEL_PERCENT             = 0x52, /* Ethanol fuel % */
    PID_01_ABSOLUTE_EVAP_SYSTEM_VAPOR_PRESS = 0x53, /* Absolute Evap system Vapor Pressure */
    PID_01_EVAP_SYSTEM_VAPOR_PRESSURE_2     = 0x54, /* Evap system vapor pressure */
    PID_01_SHORT_TERM_SEC_O2_TRIM_BANK1_3   = 0x55, /* Short term secondary oxygen sensor trim, A: bank 1, B: bank 3 */
    PID_01_LONG_TERM_SEC_O2_TRIM_BANK1_3    = 0x56, /* Long term secondary oxygen sensor trim, A: bank 1, B: bank 3 */
    PID_01_SHORT_TERM_SEC_O2_TRIM_BANK2_4   = 0x57, /* Short term secondary oxygen sensor trim, A: bank 2, B: bank 4 */
    PID_01_LONG_TERM_SEC_O2_TRIM_BANK2_4    = 0x58, /* Long term secondary oxygen sensor trim, A: bank 2, B: bank 4 */
    PID_01_FUEL_RAIL_ABS_PRESSURE           = 0x59, /* Fuel rail absolute pressure */
    PID_01_RELATIVE_ACCEL_PEDAL_POSITION    = 0x5A, /* Relative accelerator pedal position */
    PID_01_HYBRID_BATTERY_PACK_REMAINING    = 0x5B, /* Hybrid battery pack remaining life */
    PID_01_ENGINE_OIL_TEMP                  = 0x5C, /* Engine oil temperature */
    PID_01_FUEL_INJECTION_TIMING            = 0x5D, /* Fuel injection timing */
    PID_01_ENGINE_FUEL_RATE                 = 0x5E, /* Engine fuel rate */
    PID_01_EMISSION_REQUIREMENTS            = 0x5F, /* Emission requirements to which vehicle is designed */

    /* PIDs supported [61-80] */
    PID_01_PIDS_SUPPORTED_61_80             = 0x60, /* PIDs supported [61-80] */
    PID_01_DEMANDED_ENGINE_TORQUE_PERCENT   = 0x61, /* Driver's demand engine - percent torque */
    PID_01_ACTUAL_ENGINE_TORQUE_PERCENT     = 0x62, /* Actual engine - percent torque */
    PID_01_ENGINE_REFERENCE_TORQUE          = 0x63, /* Engine reference torque */
    PID_01_ENGINE_PERCENT_TORQUE_DATA       = 0x64, /* Engine percent torque data */
    PID_01_AUX_INPUT_OUTPUT_SUPPORTED       = 0x65, /* Auxiliary input / output supported */
    PID_01_MAF_SENSOR                       = 0x66, /* Mass air flow sensor */
    PID_01_ENGINE_COOLANT_TEMP              = 0x67, /* Engine coolant temperature */
    PID_01_INTAKE_AIR_TEMP_SENSOR           = 0x68, /* Intake air temperature sensor */
    PID_01_COMMANDED_EGR_AND_EGR_ERROR      = 0x69, /* Commanded EGR and EGR Error */
    PID_01_COMMANDED_DIESEL_INTAKE_AIR      = 0x6A, /* Commanded Diesel intake air flow control and relative intake air flow position */
    PID_01_EGR_TEMP                         = 0x6B, /* Exhaust gas recirculation temperature */
    PID_01_COMMANDED_THROTTLE_ACTUATOR_2    = 0x6C, /* Commanded throttle actuator control and relative throttle position */
    PID_01_FUEL_PRESSURE_CONTROL_SYSTEM     = 0x6D, /* Fuel pressure control system */
    PID_01_INJECTION_PRESSURE_CTRL_SYSTEM   = 0x6E, /* Injection pressure control system */
    PID_01_TURBO_COMPRESSOR_INLET_PRESSURE  = 0x6F, /* Turbocharger compressor inlet pressure */
    PID_01_BOOST_PRESSURE_CONTROL           = 0x70, /* Boost pressure control */
    PID_01_VGT_CONTROL                      = 0x71, /* Variable Geometry turbo (VGT) control */
    PID_01_WASTEGATE_CONTROL                = 0x72, /* Wastegate control */
    PID_01_EXHAUST_PRESSURE                 = 0x73, /* Exhaust pressure */
    PID_01_TURBO_RPM                        = 0x74, /* Turbocharger RPM */
    PID_01_TURBO_TEMP_1                     = 0x75, /* Turbocharger temperature */
    PID_01_TURBO_TEMP_2                     = 0x76, /* Turbocharger temperature */
    PID_01_CHARGE_AIR_COOLER_TEMP           = 0x77, /* Charge air cooler temperature (CACT) */
    PID_01_EGT_BANK1                        = 0x78, /* Exhaust Gas temperature (EGT) Bank 1 */
    PID_01_EGT_BANK2                        = 0x79, /* Exhaust Gas temperature (EGT) Bank 2 */
    PID_01_DPF_1                            = 0x7A, /* Diesel particulate filter (DPF) */
    PID_01_DPF_2                            = 0x7B, /* Diesel particulate filter (DPF) */
    PID_01_DPF_TEMP                         = 0x7C, /* Diesel Particulate filter (DPF) temperature */
    PID_01_NOX_NTE_CONTROL_AREA_STATUS      = 0x7D, /* NOx NTE (Not-To-Exceed) control area status */
    PID_01_PM_NTE_CONTROL_AREA_STATUS       = 0x7E, /* PM NTE (Not-To-Exceed) control area status */
    PID_01_ENGINE_RUN_TIME                  = 0x7F, /* Engine run time */

    /* PIDs supported [81-A0] */
    PID_01_PIDS_SUPPORTED_81_A0             = 0x80, /* PIDs supported [81-A0] */
    PID_01_ENGINE_RUN_TIME_AECD_1           = 0x81, /* Engine run time for Auxiliary Emissions Control Device (AECD) */
    PID_01_ENGINE_RUN_TIME_AECD_2           = 0x82, /* Engine run time for Auxiliary Emissions Control Device (AECD) */
    PID_01_NOX_SENSOR                       = 0x83, /* NOx sensor */
    PID_01_MANIFOLD_SURFACE_TEMP            = 0x84, /* Manifold surface temperature */
    PID_01_NOX_REAGENT_SYSTEM               = 0x85, /* NOx reagent system */
    PID_01_PM_SENSOR                        = 0x86, /* Particulate matter (PM) sensor */
    PID_01_INTAKE_MANIFOLD_ABS_PRESSURE     = 0x87, /* Intake manifold absolute pressure */
    PID_01_SCR_INDUCEMENT_SYSTEM            = 0x88, /* SCR Induce System */
    PID_01_RUN_TIME_AECD_11_15              = 0x89, /* Run Time for AECD #11-#15 */
    PID_01_RUN_TIME_AECD_16_20              = 0x8A, /* Run Time for AECD #16-#20 */
    PID_01_DIESEL_AFTERTREATMENT            = 0x8B, /* Diesel Aftertreatment */
    PID_01_O2_SENSOR_WIDE_RANGE             = 0x8C, /* O2 Sensor (Wide Range) */
    PID_01_THROTTLE_POSITION_G              = 0x8D, /* Throttle Position G */
    PID_01_ENGINE_FRICTION_TORQUE_PERCENT   = 0x8E, /* Engine Friction - Percent Torque */
    PID_01_PM_SENSOR_BANK1_2                = 0x8F, /* PM Sensor Bank 1 & 2 */
    PID_01_WWH_OBD_VEHICLE_INFO             = 0x90, /* WWH-OBD Vehicle OBD System Information */
    PID_01_WWH_OBD_VEHICLE_INFO_2           = 0x91, /* WWH-OBD Vehicle OBD System Information */
    PID_01_FUEL_SYSTEM_CONTROL              = 0x92, /* Fuel System Control */
    PID_01_WWH_OBD_COUNTERS                 = 0x93, /* WWH-OBD Vehicle OBD Counters support */
    PID_01_NOX_WARNING_INDUCEMENT           = 0x94, /* NOx Warning And Inducement System */
    /* 0x95-0x9F Reserved */

    /* PIDs supported [A1-C0] */
    PID_01_PIDS_SUPPORTED_A1_C0             = 0xA0, /* PIDs supported [A1-C0] */
    PID_01_NOX_SENSOR_CORRECTED             = 0xA1, /* NOx Sensor Corrected */
    PID_01_CYLINDER_FUEL_RATE               = 0xA2, /* Cylinder Fuel Rate */
    PID_01_EVAP_SYSTEM_VAPOR_PRESSURE_3     = 0xA3, /* Evap System Vapor Pressure */
    PID_01_TRANSMISSION_ACTUAL_GEAR         = 0xA4, /* Transmission Actual Gear */
    PID_01_DIESEL_EXHAUST_FLUID_DOSING      = 0xA5, /* Diesel Exhaust Fluid Dosing */
    PID_01_ODOMETER                         = 0xA6, /* Odometer */
    /* 0xA7-0xBF Reserved */

    /* PIDs supported [C1-E0] */
    PID_01_PIDS_SUPPORTED_C1_E0             = 0xC0, /* PIDs supported [C1-E0] */
    /* 0xC1-0xC2 Special PIDs */
    PID_01_FUEL_RATE_MODEL_BASED            = 0xC3, /* Fuel Rate (Model Based) - new in latest spec */
    PID_01_EXHAUST_FLOW_RATE                = 0xC4, /* Exhaust Flow Rate - new in latest spec */
    /* 0xC5-0xDF Reserved */
} service01_pids_t;

/**
 * @brief Service $02 - Show Freeze Frame Data PIDs
 * Uses same PIDs as Service $01 but returns freeze frame data
 */
typedef service01_pids_t service02_pids_t;

/**
 * @brief Service $05 - Oxygen Sensor Monitoring Test Results (non-CAN)
 */
typedef enum {
    PID_05_O2_SENSOR_MONITOR_BANK1_SENSOR1  = 0x01, /* O2 Sensor Monitor Bank 1 - Sensor 1 */
    PID_05_O2_SENSOR_MONITOR_BANK1_SENSOR2  = 0x02, /* O2 Sensor Monitor Bank 1 - Sensor 2 */
    PID_05_O2_SENSOR_MONITOR_BANK1_SENSOR3  = 0x03, /* O2 Sensor Monitor Bank 1 - Sensor 3 */
    PID_05_O2_SENSOR_MONITOR_BANK1_SENSOR4  = 0x04, /* O2 Sensor Monitor Bank 1 - Sensor 4 */
    PID_05_O2_SENSOR_MONITOR_BANK2_SENSOR1  = 0x05, /* O2 Sensor Monitor Bank 2 - Sensor 1 */
    PID_05_O2_SENSOR_MONITOR_BANK2_SENSOR2  = 0x06, /* O2 Sensor Monitor Bank 2 - Sensor 2 */
    PID_05_O2_SENSOR_MONITOR_BANK2_SENSOR3  = 0x07, /* O2 Sensor Monitor Bank 2 - Sensor 3 */
    PID_05_O2_SENSOR_MONITOR_BANK2_SENSOR4  = 0x08, /* O2 Sensor Monitor Bank 2 - Sensor 4 */
    PID_05_O2_SENSOR_MONITOR_BANK3_SENSOR1  = 0x09, /* O2 Sensor Monitor Bank 3 - Sensor 1 */
    PID_05_O2_SENSOR_MONITOR_BANK3_SENSOR2  = 0x0A, /* O2 Sensor Monitor Bank 3 - Sensor 2 */
    PID_05_O2_SENSOR_MONITOR_BANK3_SENSOR3  = 0x0B, /* O2 Sensor Monitor Bank 3 - Sensor 3 */
    PID_05_O2_SENSOR_MONITOR_BANK3_SENSOR4  = 0x0C, /* O2 Sensor Monitor Bank 3 - Sensor 4 */
    PID_05_O2_SENSOR_MONITOR_BANK4_SENSOR1  = 0x0D, /* O2 Sensor Monitor Bank 4 - Sensor 1 */
    PID_05_O2_SENSOR_MONITOR_BANK4_SENSOR2  = 0x0E, /* O2 Sensor Monitor Bank 4 - Sensor 2 */
    PID_05_O2_SENSOR_MONITOR_BANK4_SENSOR3  = 0x0F, /* O2 Sensor Monitor Bank 4 - Sensor 3 */
    PID_05_O2_SENSOR_MONITOR_BANK4_SENSOR4  = 0x10, /* O2 Sensor Monitor Bank 4 - Sensor 4 */
} service05_pids_t;

/**
 * @brief Service $06 - Test Results, Other Monitoring (CAN only)
 */
typedef enum {
    PID_06_MIDS_SUPPORTED_01_20             = 0x00, /* MIDs supported [01-20] */
    PID_06_O2_SENSOR_MONITOR_BANK1_SENSOR1  = 0x01, /* O2 Sensor Monitor Bank 1 - Sensor 1 */
    PID_06_O2_SENSOR_MONITOR_BANK1_SENSOR2  = 0x02, /* O2 Sensor Monitor Bank 1 - Sensor 2 */
    PID_06_O2_SENSOR_MONITOR_BANK2_SENSOR1  = 0x03, /* O2 Sensor Monitor Bank 2 - Sensor 1 */
    PID_06_O2_SENSOR_MONITOR_BANK2_SENSOR2  = 0x04, /* O2 Sensor Monitor Bank 2 - Sensor 2 */
    PID_06_O2_SENSOR_MONITOR_BANK3_SENSOR1  = 0x05, /* O2 Sensor Monitor Bank 3 - Sensor 1 */
    PID_06_O2_SENSOR_MONITOR_BANK3_SENSOR2  = 0x06, /* O2 Sensor Monitor Bank 3 - Sensor 2 */
    PID_06_O2_SENSOR_MONITOR_BANK4_SENSOR1  = 0x07, /* O2 Sensor Monitor Bank 4 - Sensor 1 */
    PID_06_O2_SENSOR_MONITOR_BANK4_SENSOR2  = 0x08, /* O2 Sensor Monitor Bank 4 - Sensor 2 */
    /* ... additional MIDs */
    PID_06_MIDS_SUPPORTED_21_40             = 0x20, /* MIDs supported [21-40] */
    PID_06_CATALYST_MONITOR_BANK1           = 0x21, /* Catalyst Monitor Bank 1 */
    PID_06_CATALYST_MONITOR_BANK2           = 0x22, /* Catalyst Monitor Bank 2 */
    /* ... */
    PID_06_MIDS_SUPPORTED_41_60             = 0x40, /* MIDs supported [41-60] */
    PID_06_EVAP_MONITOR                     = 0x41, /* Evap Monitor (Cap Off / 0.020") */
    PID_06_EVAP_MONITOR_2                   = 0x42, /* Evap Monitor (0.040") */
    /* ... */
    PID_06_MIDS_SUPPORTED_61_80             = 0x60, /* MIDs supported [61-80] */
    PID_06_O2_SENSOR_HEATER_MONITOR_B1S1    = 0x61, /* O2 Sensor Heater Monitor Bank 1 - Sensor 1 */
    PID_06_O2_SENSOR_HEATER_MONITOR_B1S2    = 0x62, /* O2 Sensor Heater Monitor Bank 1 - Sensor 2 */
    PID_06_O2_SENSOR_HEATER_MONITOR_B2S1    = 0x63, /* O2 Sensor Heater Monitor Bank 2 - Sensor 1 */
    PID_06_O2_SENSOR_HEATER_MONITOR_B2S2    = 0x64, /* O2 Sensor Heater Monitor Bank 2 - Sensor 2 */
    /* ... */
    PID_06_MIDS_SUPPORTED_81_A0             = 0x80, /* MIDs supported [81-A0] */
    PID_06_MIDS_SUPPORTED_A1_C0             = 0xA0, /* MIDs supported [A1-C0] */
    PID_06_MIDS_SUPPORTED_C1_E0             = 0xC0, /* MIDs supported [C1-E0] */
    PID_06_MIDS_SUPPORTED_E1_FF             = 0xE0, /* MIDs supported [E1-FF] */
} service06_mids_t;

/**
 * @brief Service $08 - Control Operation of On-Board Component/System TIDs
 */
typedef enum {
    PID_08_TIDS_SUPPORTED_01_20             = 0x00, /* TIDs supported [01-20] */
    PID_08_EVAP_LEAK_TEST                   = 0x01, /* Evaporative system leak test */
    PID_08_EVAP_SEAL_TEST                   = 0x02, /* Evaporative system seal test */
    /* Additional TIDs as defined by manufacturer */
} service08_tids_t;

/**
 * @brief Service $09 - Request Vehicle Information PIDs
 */
typedef enum {
    PID_09_INFOTYPE_SUPPORTED_01_20         = 0x00, /* InfoType supported [01-20] */
    PID_09_VIN_MESSAGE_COUNT                = 0x01, /* VIN Message Count */
    PID_09_VIN                              = 0x02, /* Vehicle Identification Number (VIN) */
    PID_09_CALID_MESSAGE_COUNT              = 0x03, /* Calibration ID message count */
    PID_09_CALID                            = 0x04, /* Calibration ID */
    PID_09_CVN_MESSAGE_COUNT                = 0x05, /* Calibration verification numbers (CVN) message count */
    PID_09_CVN                              = 0x06, /* Calibration Verification Numbers (CVN) */
    PID_09_IPTCOUNT_MSG_COUNT               = 0x07, /* In-use performance tracking message count */
    PID_09_INUSE_PERF_TRACKING              = 0x08, /* In-use performance tracking for spark ignition vehicles */
    PID_09_ECU_NAME_MESSAGE_COUNT           = 0x09, /* ECU name message count */
    PID_09_ECU_NAME                         = 0x0A, /* ECU name */
    PID_09_INUSE_PERF_TRACKING_COMPRESSION  = 0x0B, /* In-use performance tracking for compression ignition vehicles */
    PID_09_ESN_MESSAGE_COUNT                = 0x0C, /* ESN message count */
    PID_09_ESN                              = 0x0D, /* ESN */
    PID_09_EROTAN_MESSAGE_COUNT             = 0x0E, /* EROTAN message count */
    PID_09_EROTAN                           = 0x0F, /* EROTAN */
    /* 0x10-0x1F Reserved */
    PID_09_INFOTYPE_SUPPORTED_21_40         = 0x20, /* InfoType supported [21-40] */
} service09_pids_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* ISO15031_5_H */
