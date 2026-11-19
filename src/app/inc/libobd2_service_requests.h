#ifndef LIBOBD2_SERVICE_REQUESTS_H
#define LIBOBD2_SERVICE_REQUESTS_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include <stdint.h>
#include "iso15031_5.h"

/* ================================================= MACROS ================================================ */
#define LIBOBD2_MAX_SREQ  (10U) /* requests one service can send */
#define LIBOBD2_FREEZE_FRAME_NR (0x00U) /* the only freeze frame ISO 15031-5 defines */

/* One request with no parameter: sid */
#define SREQ_SID(s) \
    { \
        [0] = { .data = { .sid = (s) }, .dataLen = 1 } \
    }

/* One request for one PID: sid PID */
#define SREQ_PID(s, p) \
    { \
        [0] = { .data = { .sid = (s), .param = { (p) } }, .dataLen = 2 } \
    }

/* One $02 request: 02 PID frame */
#define SREQ_FREEZE_FRAME(p) \
    { \
        [0] = { .data = { .sid = SID_SHOW_FREEZE_FRAME_DATA, .param = { (p), LIBOBD2_FREEZE_FRAME_NR } }, .dataLen = 3 } \
    }

/* The two $09 requests of a multi-message InfoType: the message count, then the data */
#define SREQ_VEHICLE_INFO(cntPid, p) \
    { \
        [0] = { .data = { .sid = SID_REQUEST_VEHICLE_INFO, .param = { (cntPid) } }, .dataLen = 2 }, \
        [1] = { .data = { .sid = SID_REQUEST_VEHICLE_INFO, .param = { (p) } }, .dataLen = 2 } \
    }

#define SREQ_GET_VIN          SREQ_VEHICLE_INFO(PID_09_VIN_MESSAGE_COUNT, PID_09_VIN)
#define SREQ_GET_CALID        SREQ_VEHICLE_INFO(PID_09_CALID_MESSAGE_COUNT, PID_09_CALID)
#define SREQ_GET_CVN          SREQ_VEHICLE_INFO(PID_09_CVN_MESSAGE_COUNT, PID_09_CVN)
#define SREQ_GET_IPT          SREQ_VEHICLE_INFO(PID_09_IPTCOUNT_MSG_COUNT, PID_09_INUSE_PERF_TRACKING)
#define SREQ_GET_COOLANT_TEMP SREQ_PID(SID_SHOW_CURRENT_DATA, PID_01_COOLANT_TEMP)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    obd_data_t data;
    size_t dataLen;
} libobd2_data_t;

typedef struct
{
    libobd2_data_t serviceRequests[LIBOBD2_MAX_SREQ]; // tester requests
}libobd2_sreq_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_SERVICE_REQUESTS_H */
