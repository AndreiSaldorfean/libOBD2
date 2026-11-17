#ifndef ECU_LIBOBD2_SERVICE_REQUESTS_H
#define ECU_LIBOBD2_SERVICE_REQUESTS_H

/*************************************************************************************************************
 * What the simulated ECU answers. Each answer is the list of messages it sends back for one request,
 * as they go on the line (sid + params, no header, no checksum).
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include <stdint.h>
#include "iso15031_5.h"
#include "libobd2_service_requests.h"

/* ================================================= MACROS ================================================ */
#define ECU_MASK (0x40)

/* One message: positive response sid s + the params. dataLen counts the sid. */
#define ECU_MSG(s, ...) \
    { .data = { .sid = (uint8_t)((s) + ECU_MASK), .param = { __VA_ARGS__ } }, .dataLen = 1U + sizeof((uint8_t[]){ __VA_ARGS__ }) }

/* One message without params */
#define ECU_MSG_SID(s) \
    { .data = { .sid = (uint8_t)((s) + ECU_MASK) }, .dataLen = 1U }

/* The messages of one answer, counted */
#define ECU_ANSWER(...) \
    { \
        .serviceRequests = { .serviceRequests = { __VA_ARGS__ } }, \
        .sreqLen = sizeof((libobd2_data_t[]){ __VA_ARGS__ }) / sizeof(libobd2_data_t) \
    }

/* A $01 answer (41 PID A..) and the $02 answer for freeze frame 0 (42 PID 00 A..) */
#define ECU_LIVE(pid, ...)   ECU_ANSWER(ECU_MSG(SID_SHOW_CURRENT_DATA, (pid), __VA_ARGS__))
#define ECU_FREEZE(pid, ...) ECU_ANSWER(ECU_MSG(SID_SHOW_FREEZE_FRAME_DATA, (pid), LIBOBD2_FREEZE_FRAME_NR, __VA_ARGS__))

/* One message of a $09 multi-message answer: 49 PID msgNr D1 D2 D3 D4 */
#define ECU_INFO_MSG(pid, nr, d1, d2, d3, d4) ECU_MSG(SID_REQUEST_VEHICLE_INFO, (pid), (nr), (d1), (d2), (d3), (d4))

/* ---- $09: VIN "WVWZZZ1JZXW000001", always 5 messages on ISO 9141-2 (Annex G, Table G.2) ---- */
#define ECU_ANSWER_VIN_MESSAGE_COUNT ECU_ANSWER(ECU_MSG(SID_REQUEST_VEHICLE_INFO, PID_09_VIN_MESSAGE_COUNT, 0x05U))
#define ECU_ANSWER_VIN ECU_ANSWER( \
        ECU_INFO_MSG(PID_09_VIN, 0x1, 0x00, 0x00, 0x00, 'W'), \
        ECU_INFO_MSG(PID_09_VIN, 0x2, 'V', 'W', 'Z', 'Z'), \
        ECU_INFO_MSG(PID_09_VIN, 0x3, 'Z', '1', 'J', 'Z'), \
        ECU_INFO_MSG(PID_09_VIN, 0x4, 'X', 'W', '0', '0'), \
        ECU_INFO_MSG(PID_09_VIN, 0x5, '0', '0', '0', '1'))

/* ---- $09: two CALIDs "JMB*36761500" and "LIBOBD2-ECU-SIM", 4 messages each (Table G.3) ---- */
#define ECU_ANSWER_CALID_MESSAGE_COUNT ECU_ANSWER(ECU_MSG(SID_REQUEST_VEHICLE_INFO, PID_09_CALID_MESSAGE_COUNT, 0x08U))
#define ECU_ANSWER_CALID ECU_ANSWER( \
        ECU_INFO_MSG(PID_09_CALID, 0x1, 'J', 'M', 'B', '*'), \
        ECU_INFO_MSG(PID_09_CALID, 0x2, '3', '6', '7', '6'), \
        ECU_INFO_MSG(PID_09_CALID, 0x3, '1', '5', '0', '0'), \
        ECU_INFO_MSG(PID_09_CALID, 0x4, 0x00, 0x00, 0x00, 0x00), \
        ECU_INFO_MSG(PID_09_CALID, 0x5, 'L', 'I', 'B', 'O'), \
        ECU_INFO_MSG(PID_09_CALID, 0x6, 'B', 'D', '2', '-'), \
        ECU_INFO_MSG(PID_09_CALID, 0x7, 'E', 'C', 'U', '-'), \
        ECU_INFO_MSG(PID_09_CALID, 0x8, 'S', 'I', 'M', 0x00))

/* ---- $09: one CVN per CALID, 1 message each (Table G.6) ---- */
#define ECU_ANSWER_CVN_MESSAGE_COUNT ECU_ANSWER(ECU_MSG(SID_REQUEST_VEHICLE_INFO, PID_09_CVN_MESSAGE_COUNT, 0x02U))
#define ECU_ANSWER_CVN ECU_ANSWER( \
        ECU_INFO_MSG(PID_09_CVN, 0x1, 0x17, 0x91, 0xBC, 0x82), \
        ECU_INFO_MSG(PID_09_CVN, 0x2, 0x2A, 0xE0, 0x62, 0xBE))

/* ---- $09: in-use performance tracking, 16 counters, 2 per message, always 8 messages (Table G.7) ---- */
#define ECU_ANSWER_IPT_MESSAGE_COUNT ECU_ANSWER(ECU_MSG(SID_REQUEST_VEHICLE_INFO, PID_09_IPTCOUNT_MSG_COUNT, 0x08U))
#define ECU_ANSWER_IPT ECU_ANSWER( \
        ECU_INFO_MSG(PID_09_INUSE_PERF_TRACKING, 0x1, 0x01, 0x2C, 0x02, 0x58), /* OBDCOND 300, IGNCNTR 600 */ \
        ECU_INFO_MSG(PID_09_INUSE_PERF_TRACKING, 0x2, 0x00, 0x64, 0x00, 0xC8), /* CATCOMP1 100, CATCOND1 200 */ \
        ECU_INFO_MSG(PID_09_INUSE_PERF_TRACKING, 0x3, 0x00, 0x32, 0x00, 0x50), /* CATCOMP2 50, CATCOND2 80 */ \
        ECU_INFO_MSG(PID_09_INUSE_PERF_TRACKING, 0x4, 0x00, 0x28, 0x00, 0x50), /* O2SCOMP1 40, O2SCOND1 80 */ \
        ECU_INFO_MSG(PID_09_INUSE_PERF_TRACKING, 0x5, 0x00, 0x30, 0x00, 0x50), /* O2SCOMP2 48, O2SCOND2 80 */ \
        ECU_INFO_MSG(PID_09_INUSE_PERF_TRACKING, 0x6, 0x00, 0x29, 0x00, 0x50), /* EGRCOMP 41, EGRCOND 80 */ \
        ECU_INFO_MSG(PID_09_INUSE_PERF_TRACKING, 0x7, 0x00, 0x00, 0x00, 0x00), /* AIR: not fitted */ \
        ECU_INFO_MSG(PID_09_INUSE_PERF_TRACKING, 0x8, 0x00, 0x00, 0x00, 0x00)) /* EVAP: not fitted */

/* ---- $03 / $07 / $0A: 3 DTC slots per message, $0000 fill ---- */
/* stored: P0143 U0196 P0234 B0123 */
#define ECU_ANSWER_STORED_DTCS ECU_ANSWER( \
        ECU_MSG(SID_SHOW_STORED_DTCS, 0x01, 0x43, 0xC1, 0x96, 0x02, 0x34), \
        ECU_MSG(SID_SHOW_STORED_DTCS, 0x81, 0x23, 0x00, 0x00, 0x00, 0x00))
/* pending: P0300 */
#define ECU_ANSWER_PENDING_DTCS ECU_ANSWER(ECU_MSG(SID_SHOW_PENDING_DTCS, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00))
/* permanent: P0143 */
#define ECU_ANSWER_PERMANENT_DTCS ECU_ANSWER(ECU_MSG(SID_PERMANENT_DTCS, 0x01, 0x43, 0x00, 0x00, 0x00, 0x00))

/* ---- $04 ---- */
#define ECU_ANSWER_CLEAR_DTCS ECU_ANSWER(ECU_MSG_SID(SID_CLEAR_DTCS))

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* ECU_LIBOBD2_SERVICE_REQUESTS_H */
