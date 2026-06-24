#ifndef ECU_LIBOBD2_SERVICE_REQUESTS_H
#define ECU_LIBOBD2_SERVICE_REQUESTS_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include <stdint.h>
#include "iso15031_5.h"

/* ================================================= MACROS ================================================ */
#define ECU_MASK (0x40)

#define SREQ_SEND_VIN_MESSAGE_COUNT \
    ((libobd2_sreq_t){.serviceRequests = {{ .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN_MESSAGE_COUNT, 0x8U}, .dataLen = 3}}})

#define SREQ_SEND_CALID_MESSAGE_COUNT \
    ((libobd2_sreq_t){.serviceRequests = {{ .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID_MESSAGE_COUNT, 0x8U}, .dataLen = 3}}})


#define SREQ_SEND_VIN ((libobd2_sreq_t){ .serviceRequests = \
    {\
        [0] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN, 0x1, 0x37, 0x35, 0x32, 0x36}, .dataLen = 7}, \
        [1] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN, 0x2, 0x37, 0x35, 0x33, 0x00}, .dataLen = 7}, \
        [2] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN, 0x3, 0x00, 0x00, 0x00, 0x00}, .dataLen = 7}, \
        [3] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN, 0x4, 0x00, 0x00, 0x00, 0x00}, .dataLen = 7}, \
        [4] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN, 0x5, 0x37, 0x35, 0x32, 0x38}, .dataLen = 7}, \
        [5] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN, 0x6, 0x36, 0x36, 0x37, 0x00}, .dataLen = 7}, \
        [6] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN, 0x7, 0x00, 0x00, 0x00, 0x00}, .dataLen = 7}, \
        [7] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_VIN, 0x8, 0x00, 0x00, 0x00, 0x00}, .dataLen = 7}, \
    }})

#define SREQ_SEND_CALID ((libobd2_sreq_t){ .serviceRequests = \
    {\
        [0] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID, 0x1, 0x37, 0x35, 0x32, 0x36}, .dataLen = 7}, \
        [1] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID, 0x2, 0x37, 0x35, 0x33, 0x00}, .dataLen = 7}, \
        [2] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID, 0x3, 0x00, 0x00, 0x00, 0x00}, .dataLen = 7}, \
        [3] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID, 0x4, 0x00, 0x00, 0x00, 0x00}, .dataLen = 7}, \
        [4] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID, 0x5, 0x37, 0x35, 0x32, 0x38}, .dataLen = 7}, \
        [5] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID, 0x6, 0x36, 0x36, 0x37, 0x00}, .dataLen = 7}, \
        [6] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID, 0x7, 0x00, 0x00, 0x00, 0x00}, .dataLen = 7}, \
        [7] = { .data.sid = SID_REQUEST_VEHICLE_INFO + ECU_MASK, .data.param = {PID_09_CALID, 0x8, 0x00, 0x00, 0x00, 0x00}, .dataLen = 7}, \
    }})

#define SREQ_SEND_COOLANT_TEMP ((libobd2_sreq_t){.serviceRequests = {{ .data.sid = SID_SHOW_CURRENT_DATA + ECU_MASK, .data.param = {PID_01_COOLANT_TEMP, 0x9A}, .dataLen = 3 }}})

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* ECU_LIBOBD2_SERVICE_REQUESTS_H */
