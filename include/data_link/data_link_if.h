#ifndef DATA_LINK_H
#define DATA_LINK_H

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"
#include "timing_if.h"
#include "transport_if.h"
#include <stddef.h>

/* ================================================= MACROS ================================================ */
#define LIBOBD_TimingInit(handle)            (handle->pTimingOps->timer_init(handle->pTimingHandle))
#define LIBOBD_GetTimeMs(handle)             (handle->pTimingOps->get_time_ms(handle->pTimingHandle))
#define LIBOBD_Delay(handle, delay)          (handle->pTimingOps->delay_ms(handle->pTimingHandle, delay))
#define LIBOBD_StartTimeout(handle, timeout) (handle->pTimingOps->start_timeout(handle->pTimingHandle, timeout, NULL, NULL))
#define LIBOBD_StopTimeout(handle)           (handle->pTimingOps->stop_timeout(handle->pTimingHandle))
#define LIBOBD_IsTimeoutExpired(handle)      (handle->pTimingOps->is_timeout_expired(handle->pTimingHandle))

#define LIBOBD_TransportInit(handle)      (handle->pTransportOps->init(handle->pTransportHandle))
#define LIBOBD_SendByte(handle, byte)     (handle->pTransportOps->send_byte(handle->pTransportHandle, byte))
#define LIBOBD_ReceiveByte(handle, byte)  (handle->pTransportOps->recv_byte(handle->pTransportHandle, byte))
#define LIBOBD_SendPulse(handle, pulse)   (handle->pTransportOps->send_pulse(handle->pTransportHandle, pulse))
#define LIBOBD_SwitchMode(handle, mode)   (handle->pTransportOps->switch_mode(handle->pTransportHandle, mode))
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
    obd_transport_ops_t *pTransportOps;
    void *pTransportHandle;

    /* Timing layer */
    obd_timing_ops_t *pTimingOps;
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
