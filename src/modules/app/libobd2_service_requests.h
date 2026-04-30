#ifndef LIBOBD2_SERVICE_REQUESTS_H
#define LIBOBD2_SERVICE_REQUESTS_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include <stdint.h>
#include "iso15031_5.h"

/* ================================================= MACROS ================================================ */
#define SREQ_GET_VIN ((libobd2_sreq_t){ .req = \
    { \
        [0] = { .data.sid = SID_REQUEST_VEHICLE_INFO, .data.param = {PID_09_VIN}, .dataLen = 1}, \
        [1] = { .data.sid = SID_REQUEST_VEHICLE_INFO, .data.param = {PID_09_CALID_MESSAGE_COUNT}, .dataLen = 1}, \
        [2] = { .data.sid = SID_REQUEST_VEHICLE_INFO, .data.param = {PID_09_CALID}, .dataLen = 1} \
    } })

#define SREQ_GET_COOLANT_TEMP ((libobd2_sreq_t){.req = {{ .data.sid = SID_SHOW_CURRENT_DATA, .data.param = {PID_01_COOLANT_TEMP}, .dataLen = 1 }}})

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    obd_data_t data;
    size_t dataLen;
} libobd2_data_t;

typedef struct
{
    libobd2_data_t req[10]; // tester requests
}libobd2_sreq_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_SERVICE_REQUESTS_H */
