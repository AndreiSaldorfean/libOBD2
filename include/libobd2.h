#ifndef L3_KWP_H
#define L3_KWP_H

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"
#include "data_link_if.h"
#include <stddef.h>
#include "stdbool.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    const dataLink_if_t *pDataLink;
    bool connectionStatus;
}obd_ctx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t LibOBD2_Init(obd_ctx_t *ctx);
obd_status_t LibOBD2_RequestService(obd_ctx_t *ctx, uint8_t sid, uint8_t pid, uint32_t *response_buffer, size_t *response_len);

#endif /* L3_KWP_H */
