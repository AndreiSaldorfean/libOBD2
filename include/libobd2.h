#ifndef LIBOBD2_H
#define LIBOBD2_H

/* ================================================ INCLUDES =============================================== */
#include "data_link_if.h"
#include "srv_status.h"
#include <stdbool.h>
#include <stddef.h>

/* ================================================= MACROS ================================================ */
// #define SPT_5BAUD_INIT
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    const dataLink_if_t *pDataLink;
    void* pDataLinkHandle;
    bool connectionStatus;
} obd_ctx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t LibOBD2_Init(obd_ctx_t *ctx);
obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    const obd_request_t* request,
    size_t requestLen,
    obd_response_t* response,
    size_t* responseLen);

#endif /* LIBOBD2_H */
