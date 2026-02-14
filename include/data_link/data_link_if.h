#ifndef DATA_LINK_H
#define DATA_LINK_H

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"
#include "timing_if.h"
#include "transport_if.h"
#include <stddef.h>

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t sid;
    uint8_t param[255];
} obd_request_t;

typedef struct
{
    union{
        struct
        {
            uint8_t sid;
            uint8_t data[255];
        }positive;

        struct
        {
            uint8_t negResp;
            uint8_t sid;
            uint8_t data[254];
        }negative;
    };
} obd_response_t;

/* Forward declaration */
typedef struct dataLink_if dataLink_if_t;

struct dataLink_if
{
    /* Transport layer */
    const obd_transport_ops_t *pTransportOps;
    void *pTransportHandle;

    /* Timing layer */
    const obd_timing_ops_t *pTimingOps;
    void *pTimingHandle;

    /* Protocol-specific context (e.g., l2_kwp_ctx_t) */
    void *pProtocolCtx;

    /* Data link operations */
    obd_status_t (*connect)(dataLink_if_t *pDataLink);
    obd_status_t (*send_request)(dataLink_if_t *pDataLink, const obd_request_t *req, size_t len);
    obd_status_t (*recv_response)(dataLink_if_t *pDataLink, obd_response_t *resp, size_t *len);
};

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* DATA_LINK_H */
