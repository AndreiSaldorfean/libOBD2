#ifndef ECU_LIBOBD2_SERVICES_H
#define ECU_LIBOBD2_SERVICES_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include <stdint.h>
#include "libobd2_service_requests.h"
#include "libobd2_transfer_protocol.h"

/* ================================================= MACROS ================================================ */
#define OBD_SMALL_SID ((libobd2_sid_t)(0xA5U))
#define OBD_BIG_SID   ((libobd2_sid_t)(0xC3))

#define OBD_SUPPORTED_SERVICES \
    [OBD_GET_VIN] = { .sidSize = OBD_BIG_SID, .sreq = SREQ_GET_VIN, .transferProtocol = TP_GET_VIN }, \
    [OBD_GET_COOLANT_TEMP] = { .sidSize = OBD_SMALL_SID, .sreq = SREQ_GET_COOLANT_TEMP }

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t sidSize;
    libobd2_sreq_t sreq;
    uint8_t transferProtocol[32]; // protocol
}libobd2_service_t;

typedef enum
{
    OBD_GET_VIN,
    OBD_GET_COOLANT_TEMP,
    OBD2_MAX_NUM_SERVICES
}libobd2_sid_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* ECU_LIBOBD2_SERVICES_H */
