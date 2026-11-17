#ifndef ECU_LIBOBD2_H
#define ECU_LIBOBD2_H

/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "ecu_datalink.h"
#include "statusRetCodes.h"
#include "ecu_libobd2_services.h"
#include <stdbool.h>
#include <stddef.h>

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t ECU_LibOBD2_Init(obd_ctx_t *ctx);
obd_status_t ECU_Listen(obd_ctx_t* ctx);
const ecu_libobd2_service_t *ECU_LibOBD2_GetAnswer(uint8_t sid, uint8_t pid);

#endif /* ECU_LIBOBD2_H */
