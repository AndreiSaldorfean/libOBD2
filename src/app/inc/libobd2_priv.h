#ifndef LIBOBD2_PRIV_H
#define LIBOBD2_PRIV_H

/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "libobd2_services.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define LIBOBD2_POSITIVE_RESP_OFFSET  (0x40U) /* sid of a positive response = request sid + 0x40 */

/** NOTE: so far the library doesn't support to send requests
 * do differnt ECUS, only to a single one, natively. Ofc its possible
 * if the user does so, but its planned in the future for the library
 * to have this support. */
#define MAX_NUM_OF_ECUS               (1)
#define LIBOBD2_PID_RANGES            (8U)    /* bitmaps 0x00, 0x20, ... 0xE0 cover PIDs 0x01..0xFF */

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t LibOBD2_ReadSupportedPids(obd_ctx_t *ctx);
bool LibOBD2_IsLibobd2SidSupported(libobd2_sid_t libobd2Sid);

#endif /* LIBOBD2_PRIV_H */
