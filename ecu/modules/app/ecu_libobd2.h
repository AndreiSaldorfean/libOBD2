#ifndef ECU_LIBOBD2_H
#define ECU_LIBOBD2_H

/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "ecu_datalink.h"
#include "statusRetCodes.h"
#include <stdbool.h>
#include <stddef.h>

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t ECU_LibOBD2_Init(obd_ctx_t *ctx);
obd_status_t ECU_LibOBD2_RequestService(
    obd_ctx_t *ctx,
    const obd_request_t* request,
    size_t requestLen,
    obd_response_t* response,
    size_t* responseLen);

#endif /* ECU_LIBOBD2_H */
