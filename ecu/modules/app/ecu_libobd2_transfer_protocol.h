#ifndef ECU_LIBOBD2_TRANSFER_PROTOCOL_H
#define ECU_LIBOBD2_TRANSFER_PROTOCOL_H

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TP_SEND          (0x1U)
#define TP_RECV          (0x2U)
#define TP_MSG_CNT       (0x3U)
#define TP_MSGS          (0x4U)
#define TP_READ_UNTIL_TO (0x5U)
#define TP_END           (0x0U)

#define TP_GET_VIN { TP_SEND, TP_SEND, TP_MSG_CNT, TP_SEND, TP_RECV, TP_END }
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* ECU_LIBOBD2_TRANSFER_PROTOCOL_H */
