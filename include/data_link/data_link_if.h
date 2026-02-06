#ifndef DATA_LINK_H
#define DATA_LINK_H

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"
#include "transport_if.h"
#include <stddef.h>

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t sid;
    uint32_t param;
    size_t len;
} obd_request_t;

typedef struct
{
    uint8_t sid;
    uint32_t *data;
    size_t* data_len;
} obd_response_t;

typedef struct
{
    const obd_transport_ops_t *pTransportOps;
    void *pTransportHandle;

    obd_status_t (*connect)(void *handle);
    obd_status_t (*send_request)(void *handle, obd_request_t *req);
    obd_status_t (*recv_response)(void *handle, obd_response_t *resp);
} dataLink_if_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* DATA_LINK_H */
