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
#include <stdio.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define ISO9141_MAX_DATA_FIELD_LEN   (((uint8_t)(sizeof(obd_data_t)) + 1U))
#define ISO9141_MAX_REQ_DATA_LEN (7U) /* sid + 6 params */
#define ISO9141_HEADER_LEN       (3U) /* fmt, target, source */
#define ISO9141_WIRE_OVERHEAD    (ISO9141_HEADER_LEN + 1U) /* header + checksum */
#define ISO9141_MAX_FRAME_LEN    (ISO9141_MAX_REQ_DATA_LEN + ISO9141_WIRE_OVERHEAD)
#define DL_ISO9141_HEADR         ((header_t){.fmt = 0x68, .trgAddr = 0x6A, .srcAddr = 0xF1})
#define DL_KWP2000_HEADR         ((header_t){.fmt = 0xC0, .trgAddr = 0x33, .srcAddr = 0xF1})
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* L2_ISO9141_PRIV_H */
