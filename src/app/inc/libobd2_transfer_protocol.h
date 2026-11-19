#ifndef LIBOBD2_TRANSFER_PROTOCOL_H
#define LIBOBD2_TRANSFER_PROTOCOL_H

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TP_SEND          (0x1U) /* send the next request of the service */
#define TP_RECV          (0x2U) /* receive one message */
#define TP_MSG_CNT       (0x3U) /* receive a $09 count answer, remember the count */
#define TP_MSGS          (0x4U) /* receive as many messages as the count announced */
#define TP_READ_UNTIL_TO (0x5U) /* receive messages until no answer starts within P2 max */
#define TP_END           (0x0U)

#define TP_GET_VIN   { TP_SEND, TP_MSG_CNT, TP_SEND, TP_MSGS, TP_END }
#define TP_GET_CALID { TP_SEND, TP_MSG_CNT, TP_SEND, TP_MSGS, TP_END }
#define TP_GET_CVN   { TP_SEND, TP_MSG_CNT, TP_SEND, TP_MSGS, TP_END }
#define TP_GET_IPT   { TP_SEND, TP_MSG_CNT, TP_SEND, TP_MSGS, TP_END }

/* One request, then every answer until the line stays silent: for answers of unknown number (several
 * ECUs, several components) */
#define TP_READ_ALL  { TP_SEND, TP_READ_UNTIL_TO, TP_END }

/* Each ECU with DTCs answers with one or more messages, an ECU without DTCs may not answer at all
 * (ISO 15031-5 6.3.1), so the number of answers is unknown */
#define TP_GET_DTCS  TP_READ_ALL

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_TRANSFER_PROTOCOL_H */
