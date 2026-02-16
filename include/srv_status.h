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
#define OBD_NOT_SUPPORTED            (OBD_MAKE_ERROR(SRV_STATUS_MODULE, (obd_status_t)0x3U))
#define OBD_ERR_TIMEOUT              (OBD_MAKE_ERROR(SRV_STATUS_MODULE, (obd_status_t)0x4U))

// Init err codes
#define SRV_STATUS_INIT               (0x02)
#define OBD_ERR_5BAUD_WRONG_SYNC_BYTE (OBD_MAKE_ERROR(SRV_STATUS_INIT, (obd_status_t)0x1U))
#define OBD_ERR_5BAUD_WRONG_INV_ADDR  (OBD_MAKE_ERROR(SRV_STATUS_INIT, (obd_status_t)0x2U))

// Comm err codes
#define SRV_STATUS_COMM                    (0x03)
#define OBD_ERR_COMM_P2_TIMEOUT_TESTER_ECU (OBD_MAKE_ERROR(SRV_STATUS_COMM, (obd_status_t)0x1U))
#define OBD_ERR_COMM_P3_TIMEOUT_ECU_TESTER (OBD_MAKE_ERROR(SRV_STATUS_COMM, (obd_status_t)0x2U))

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef uint16_t obd_status_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* SRV_STATUS_H */
