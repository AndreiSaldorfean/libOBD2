#ifndef SRV_STATUS_H
#define SRV_STATUS_H

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define SRV_STATUS_MODULE            (0x01)
#define OBD_MAKE_ERROR(module, code) ((obd_status_t)(((module) << 8) | ((code) & 0xFF)))
#define OBD_STATUS_OK                ((obd_status_t)0x0U)
#define OBD_GENERIC_ERROR            (OBD_MAKE_ERROR(SRV_STATUS_MODULE, (obd_status_t)0x1U))
#define OBD_ERR_NULL_PTR             (OBD_MAKE_ERROR(SRV_STATUS_MODULE, (obd_status_t)0x2U))

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef uint16_t obd_status_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* SRV_STATUS_H */
