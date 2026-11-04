#ifndef L2_ISO9141_PRIV_H
#define L2_ISO9141_PRIV_H

/* NOTE: Module-internal header. Include it only from l2_iso9141_*.c */

/* ================================================ INCLUDES =============================================== */
#include "l2_iso9141.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "libobd2.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <stddef.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define ISO9141_MAX_DATA_FIELD_LEN   (((uint8_t)(sizeof(obd_data_t)) + 1U))
#define ISO9141_MAX_REQ_DATA_LEN (7U) /* sid + 6 params */
#define ISO9141_HEADER_LEN       (3U) /* fmt, target, source */
#define ISO9141_WIRE_OVERHEAD    (ISO9141_HEADER_LEN + 1U) /* header + checksum */
#define ISO9141_MAX_FRAME_LEN    (ISO9141_MAX_REQ_DATA_LEN + ISO9141_WIRE_OVERHEAD)
#define DL_ISO9141_HEADR         ((header_t){.fmt = 0x68, .trgAddr = 0x6A, .srcAddr = 0xF1})
#define DL_KWP2000_HEADR         ((header_t){.fmt = 0xC0, .trgAddr = 0x33, .srcAddr = 0xF1})

/* 5-baud init */
#define ISO9141_INIT_TARGET_ADDR (0x33U) /* address sent at 5 baud to wake up the ECU */
#define ISO9141_INIT_BAUD_RATE   (5U)
#define ISO9141_SYNC_BYTE        (0x55U)
#define ISO9141_KB_ISO9141       (0x08U) /* KB1 = KB2 = 0x08: ISO9141-2 header */
#define ISO9141_KB_KWP2000       (0x94U) /* KB1 = KB2 = 0x94: KWP2000 header */

/* Response frame */
#define ISO9141_FMT_LEN_MASK     (0x3FU) /* length bits of a KWP2000 format byte; 0 means a length byte follows */
#define ISO9141_HDR_LEN_IDX      (offsetof(header_t, len)) /* index of header.len in message_t */
#define ISO9141_NEGATIVE_RESP_SID (0x7FU)
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* L2_ISO9141_PRIV_H */
