#ifndef LIBOBD2_H
#define LIBOBD2_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "statusRetCodes.h"
#include <stdbool.h>
#include <stddef.h>
#include "libobd2_services.h"
#include "libobd2_formulas.h" /* output types and LibOBD2_DtcToString for the caller */

/* ================================================= MACROS ================================================ */
#if defined(SPT_FREERTOS)
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#define YIELD
#else
#define YIELD
#endif
#define LIBOBD2_CONTROL_DATA_LEN (5U) /* DATA_A..DATA_E of a $08 request / answer */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    dataLink_if_t *pDataLink;
    bool connectionStatus;
} obd_ctx_t;

/* Unit of a $05 test value (Annex C) */
typedef enum
{
    LIBOBD2_UNIT_NONE,   /* manufacturer-defined TID: value, min and max are the raw bytes */
    LIBOBD2_UNIT_VOLT,
    LIBOBD2_UNIT_SECOND,
    LIBOBD2_UNIT_HERTZ,
    LIBOBD2_UNIT_COUNT,
} libobd2_unit_t;

/* One $05 oxygen sensor test result */
typedef struct
{
    uint8_t tid;
    uint8_t o2Sensor;     /* one bit set, the location as in PID $13 / $1D */
    bool hasLimits;       /* false for the constants (TIDs $01-$04): only value is reported */
    bool passed;          /* min <= value <= max; true when there are no limits */
    uint8_t rawValue, rawMin, rawMax;
    float value, min, max; /* scaled by Annex C, in unit */
    libobd2_unit_t unit;
} libobd2_o2_test_t;

/* One $06 test result: one test limit of one component (non-CAN: manufacturer scaling) */
typedef struct
{
    uint8_t tid;
    uint8_t componentId; /* bits 0-6 of the limit type & component byte */
    bool limitIsMin;     /* bit 7: limit is a minimum (else a maximum) */
    uint16_t value;
    uint16_t limit;
    bool passed;         /* value >= limit for a minimum, value <= limit for a maximum */
} libobd2_monitor_test_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t LibOBD2_Init(obd_ctx_t *ctx);
obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    libobd2_sid_t obdSid,
    uint8_t *response,
    size_t* responseLen);

/* Sends any request (sid + params, requestLen bytes) and returns every answer that comes back until none
 * starts within P2 max, as received (sid + params, no header): any service, any PID, manufacturer
 * services, every ECU that answers. No answer at all is OK with answerCnt = 0. */
obd_status_t LibOBD2_RequestRaw(
    obd_ctx_t *ctx,
    const obd_data_t *request,
    size_t requestLen,
    libobd2_data_t *answers,
    size_t answerCap,
    size_t *answerCnt);

/* Which TIDs of $05, $06 or $08 the ECU supports, in the range starting at range ($00, $20, ... $E0):
 * bit 31 = TID range + 1 ... bit 0 = TID range + $20. o2Sensor is only sent for $05. */
obd_status_t LibOBD2_RequestSupportedTids(obd_ctx_t *ctx, uint8_t sid, uint8_t range, uint8_t o2Sensor, uint32_t *supported);

/* $05: the test result of TID tid for the O2 sensor at location o2Sensor */
obd_status_t LibOBD2_RequestO2SensorTest(obd_ctx_t *ctx, uint8_t tid, uint8_t o2Sensor, libobd2_o2_test_t *result);

/* $06: every result the ECU(s) report for TID tid (one per component and limit) */
obd_status_t LibOBD2_RequestMonitorTests(obd_ctx_t *ctx, uint8_t tid, libobd2_monitor_test_t *results, size_t resultCap, size_t *resultCnt);

/* $08: asks the vehicle to run test tid (e.g. $01, evaporative system leak test) with DATA_A..E, and
 * returns the DATA_A..E it answers with. The ECU may stay silent if conditions are not right. */
obd_status_t LibOBD2_RequestControl(obd_ctx_t *ctx, uint8_t tid, const uint8_t *dataIn, uint8_t *dataOut);

/* True for the services that report their supported PIDs ($01 $02 $05 $06 $08 $09) */
bool LibOBD2_IsSidWithAvailablePids(uint8_t sid);

/* False if LibOBD2_Init found that the ECU does not answer service sid. Only the services with PIDs are
 * probed, so the others ($03 $04 $07 $0A) are always reported as supported. */
bool LibOBD2_IsSidSupportedByEcu(uint8_t sid);

#endif /* LIBOBD2_H */
