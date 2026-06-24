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

#define ECU_SRV_RESP \
    [SID_SHOW_CURRENT_DATA] = \
    { \
        [PID_01_COOLANT_TEMP] = { .serviceRequests = SREQ_SEND_COOLANT_TEMP, .sreqLen = 1 }\
    }, \
    [SID_REQUEST_VEHICLE_INFO] = \
    { \
        [PID_09_VIN_MESSAGE_COUNT] = { .serviceRequests = SREQ_SEND_CALID_MESSAGE_COUNT, .sreqLen = 1 }, \
        [PID_09_VIN] = { .serviceRequests = SREQ_SEND_VIN, .sreqLen = 8 }, \
        [PID_09_CALID_MESSAGE_COUNT] = { .serviceRequests = SREQ_SEND_CALID_MESSAGE_COUNT, .sreqLen = 1 }, \
        [PID_09_CALID] = { .serviceRequests = SREQ_SEND_CALID, .sreqLen = 8 } \
    }

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    libobd2_sreq_t serviceRequests;
    size_t sreqLen;
}ecu_libobd2_service_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* ECU_LIBOBD2_SERVICES_H */
