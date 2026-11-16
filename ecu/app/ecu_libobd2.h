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
/* Called by ECU_Listen for every request it answers, before the response is sent */
typedef void (*ecu_request_hook_t)(void *pUserData, const obd_data_t *request);

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t ECU_LibOBD2_Init(obd_ctx_t *ctx);
obd_status_t ECU_Listen(obd_ctx_t* ctx);
void ECU_SetRequestHook(ecu_request_hook_t hook, void *pUserData);
obd_status_t ECU_SetResponseParam(uint8_t sid, uint8_t pid, size_t msgIdx, size_t paramIdx, uint8_t value);

#endif /* ECU_LIBOBD2_H */
