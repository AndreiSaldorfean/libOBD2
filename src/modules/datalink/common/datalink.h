#ifndef DATA_LINK_H
#define DATA_LINK_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "timing_if.h"
#include "uart_if.h"
#include <stddef.h>
#include <string.h>
#include "utils.h"
#if defined(SPT_TRACING)
#include "trace.h"
#endif /* SPT_TRACING */

/* ================================================= MACROS ================================================ */
#define LIBOBD_SetTimeSample(handle, val)    (handle->pTimingOps->timeSample = val)
#define LIBOBD_GetTimeSample(handle)         (handle->pTimingOps->timeSample)
#define LIBOBD_TimingInit(handle)            (handle->pTimingOps->timer_init(handle->pTimingHandle))
#define LIBOBD_GetTimeMs(handle)             (handle->pTimingOps->get_time_ms(handle->pTimingHandle))
#define LIBOBD_Delay(handle, delay)          (handle->pTimingOps->delay_ms(handle->pTimingHandle, delay))
#define LIBOBD_StartTimeout(handle, timeout) (handle->pTimingOps->start_timeout(handle->pTimingHandle, timeout, NULL, NULL))
#define LIBOBD_StopTimeout(handle)           (handle->pTimingOps->stop_timeout(handle->pTimingHandle))
#define LIBOBD_IsTimeoutExpired(handle)      (handle->pTimingOps->is_timeout_expired(handle->pTimingHandle))

#define LIBOBD_TransportInit(handle)      (handle->pUartOps->init(handle->pTransportHandle))
#define LIBOBD_SendByte(handle, byte)     (handle->pUartOps->send_byte(handle->pTransportHandle, byte))
#define LIBOBD_ReceiveByte(handle, byte)  (handle->pUartOps->recv_byte(handle->pTransportHandle, byte))
#define LIBOBD_SendPulse(handle, pulse)   (handle->pUartOps->send_pulse(handle->pTransportHandle, pulse))
// #define LIBOBD_FlushRx(handle)            (handle->pUartOps->flush_rx(handle->pTransportHandle))
#define LIBOBD_FlushRx(handle)            (void)handle;
#define LIBOBD_SwitchMode(handle, mode)   (handle->pUartOps->switch_mode(handle->pTransportHandle, mode))

#define PULSE_HIGH (1U)
#define PULSE_LOW (0U)
#define MAX_BLOCKING_RECV_TIME (5000U)

#define FAST_INIT_WAKEUP_START  (0x1)
#define FAST_INIT_WAKEUP_END    (0x2)
#define DIGITAL_MODE_BEGIN   (0x3)
#define DIGITAL_MODE_END     (0x4)

// Timing
#if 0
#define P1_TIME_MIN      (0U)
#define P1_TIME_MAX      (20U)
#define P2_TIME_MIN      (25U)
#define P2_TIME_MAX      (50U)
#define P2_STAR_TIME_MIN (25U)
#define P2_STAR_TIME_MAX (5000U)
#define P3_TIME_MIN      (55U)
#define P3_TIME_MAX      (5000U)
#define P4_TIME_MIN      (5U)
#define P4_TIME_MAX      (20U)
#else
// NO TIMEOUT
#define P1_TIME_MIN      (0U)
#define P1_TIME_MAX      (5000U)
#define P2_TIME_MIN      (25U)
#define P2_TIME_MAX      (5000U)
#define P2_STAR_TIME_MIN (0U)
#define P2_STAR_TIME_MAX (5000U)
#define P3_TIME_MIN      (55U)
#define P3_TIME_MAX      (5000U)
#define P4_TIME_MIN      (5U)
#define P4_TIME_MAX      (5000U)
#endif
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t sid;
    uint8_t param[11]; // NOTE: Since only iso9141 is supported, only 7 data bytes are needed, but i left some buffer bytes so bugs will be avoided in development phase
}obd_data_t;

typedef struct header_t
{
    uint8_t fmt;
    uint8_t trgAddr;
    uint8_t srcAddr;
    uint8_t len;
} header_t;

typedef struct
{
    header_t header;
    obd_data_t data;
    uint8_t cs;
} message_t;

/* Forward declaration */
typedef struct dataLink_if dataLink_if_t;
typedef obd_status_t (*dl_connect_t)(dataLink_if_t*, uint8_t*);
typedef obd_status_t (*dl_send_request_t)(dataLink_if_t *pDataLink, const obd_data_t *serviceRequests, size_t dataLen);
typedef obd_status_t (*dl_recv_response_t)(dataLink_if_t *pDataLink, obd_data_t *resp, size_t* dataLen);

struct dataLink_if
{
    /* Uart layer */
    obd_uart_ops_t *pUartOps;
    void *pTransportHandle;

    /* Timing layer */
    obd_timing_ops_t *pTimingOps;
    void *pTimingHandle;

    /* Protocol-specific context (e.g., l2_kwp_ctx_t) */
    void *pProtocolCtx;

    /* Data link operations */
    dl_connect_t connect;
    dl_send_request_t send_request;
    dl_recv_response_t recv_response;
};

enum
{
    ISO9141,
    KWP2000
};
/* ============================================ INLINE FUNCTIONS =========================================== */
static inline void SendByteBitBanged(dataLink_if_t *self, uint8_t byte, uint8_t baudRate)
{
    const uint16_t delay = (1000 / baudRate);

    // Set line HIGH (idle) and switch to bit-bang mode for 5 baud
    LIBOBD_SwitchMode(self, DIGITAL_MODE_BEGIN);

    // Start bit (LOW)
    LIBOBD_SendPulse(self, PULSE_LOW);
    LIBOBD_Delay(self, delay);

    // Byte
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t bit = (byte >> i) & 0x01;
        LIBOBD_SendPulse(self, bit ? PULSE_HIGH : PULSE_LOW);
        LIBOBD_Delay(self, delay);
    }

    // Stop bit (HIGH)
    LIBOBD_SendPulse(self, PULSE_HIGH);
    LIBOBD_Delay(self, delay);

    // Switch to 10400 baud for response
    LIBOBD_SwitchMode(self, DIGITAL_MODE_END);
}


static inline obd_status_t ReadByteInTimeframe(dataLink_if_t *self, uint32_t *timeElapsed, uint8_t *byte, uint16_t timeMin, uint16_t timeMax)
{
    obd_status_t status  = {0};
    uint32_t timeStart   = 0;
    uint32_t timeEnd     = 0;
    uint32_t lTimeElapsed = 0;

    #if defined(SPT_TRACING)
    TRACE_TIMER_LOG(TRACE_TAG_TMR_START, timeMax);
    #endif /* SPT_TRACING */
    timeStart = LIBOBD_GetTimeMs(self);
    LIBOBD_StartTimeout(self, timeMax);

    while (!LIBOBD_ReceiveByte(self, byte))
    {
        status.timeout = OBD_ERR_TIMEOUT_MAX;
        if (LIBOBD_IsTimeoutExpired(self))
        {
            #if defined(SPT_TRACING)
            TRACE_TIMER_LOG(TRACE_TAG_TMR_EXP_MAX, timeMax);
            #endif /* SPT_TRACING */
            lTimeElapsed = timeEnd - timeStart;
            goto exit;
        }
    }

    timeEnd = LIBOBD_GetTimeMs(self);
    lTimeElapsed = timeEnd - timeStart;

    if (lTimeElapsed < timeMin)
    {
        status.timeout = OBD_ERR_TIMEOUT_MIN;
        goto exit;
    }

    memset(&status, 0, sizeof(obd_status_t));
exit:
    if (timeElapsed != NULL)
        *timeElapsed = lTimeElapsed;

    return status;
}

static inline bool RecvByteBlocking(dataLink_if_t *self, uint8_t *byte)
{
    uint32_t lTimeElapsed = 0;
    bool status = false;

    (void)lTimeElapsed;

    LIBOBD_StartTimeout(self, MAX_BLOCKING_RECV_TIME);

    while (false == status)
    {
        if (LIBOBD_IsTimeoutExpired(self))
        {
            status = false;
            goto exit;
        }

        status = LIBOBD_ReceiveByte(self, byte);
    }

exit:
    return status;
}
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t DL_Connect(dataLink_if_t *pDataLink);
obd_status_t DL_SendRequest(dataLink_if_t *pDataLink, const obd_data_t *serviceRequests, size_t dataLen);
obd_status_t DL_RecvResponse(dataLink_if_t  *handle, obd_data_t *resp, size_t* len);

#endif /* DATA_LINK_H */
