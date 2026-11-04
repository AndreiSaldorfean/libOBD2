#ifndef LIBOBD2_H
#define LIBOBD2_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "statusRetCodes.h"
#include <stdbool.h>
#include <stddef.h>
#include "libobd2_services.h"

/* ================================================= MACROS ================================================ */
#if defined(SPT_FREERTOS)
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#define YIELD
#else
#define YIELD
#endif
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    dataLink_if_t *pDataLink;
    bool connectionStatus;
} obd_ctx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t LibOBD2_Init(obd_ctx_t *ctx);
obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    libobd2_sid_t obdSid,
    uint8_t *response,
    size_t* responseLen);

#endif /* LIBOBD2_H */
