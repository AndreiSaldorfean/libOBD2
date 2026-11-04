#ifndef DATA_LINK_UTILS_H
#define DATA_LINK_UTILS_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
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
#define LIBOBD_SetTimeSample(handle, val)    (handle->timeSample = val)
#define LIBOBD_GetTimeSample(handle)         (handle->timeSample)
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


/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/******************************************
* title: DL_UartRxStatusToResponse
* brief: Maps a recv_byte result to the obd_status_t response code that
*        reports it (OBD_STATUS_OK for a clean byte).
******************************************/
static inline uint16_t DL_UartRxStatusToResponse(obd_uart_rx_status_t rxStatus)
{
    uint16_t response = OBD_GENERIC_ERROR;

    switch (rxStatus)
    {
        case OBD_UART_RX_OK:          response = OBD_STATUS_OK;        break;
        case OBD_UART_RX_NO_DATA:     response = OBD_RECV_NOT_READY;   break;
        case OBD_UART_RX_PARITY_ERR:  response = OBD_ERR_UART_PARITY;  break;
        case OBD_UART_RX_FRAMING_ERR: response = OBD_ERR_UART_FRAMING; break;
        case OBD_UART_RX_OVERRUN_ERR: response = OBD_ERR_UART_OVERRUN; break;
        case OBD_UART_RX_NOISE_ERR:   response = OBD_ERR_UART_NOISE;   break;
        default:                                                       break;
    }

    return response;
}

static inline void DL_SendByteBitBanged(dataLink_if_t *self, uint8_t byte, uint8_t baudRate)
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


/******************************************
* title: DL_ReadByteInTimeframe
* brief: Waits up to timeMax for one byte. Reports a missing or early byte in
*        .timeout and a UART receive error in .response.
******************************************/
static inline obd_status_t DL_ReadByteInTimeframe(dataLink_if_t *self, uint32_t *timeElapsed, uint8_t *byte, uint16_t timeMin, uint16_t timeMax)
{
    obd_status_t status  = {0};
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_NO_DATA;
    uint32_t timeStart   = 0;
    uint32_t timeEnd     = 0;
    uint32_t lTimeElapsed = 0;

    #if defined(SPT_TRACING)
    TRACE_TIMER_LOG(TRACE_TAG_TMR_START, timeMax);
    #endif /* SPT_TRACING */
    timeStart = LIBOBD_GetTimeMs(self);

    status.response = OBD_ERR_TMR_START_FAILED;
    OBD2_IF_COND_GOTO_EXIT(OBD_TMR_OK != LIBOBD_StartTimeout(self, timeMax));
    status.response = OBD_STATUS_OK;

    rxStatus = LIBOBD_ReceiveByte(self, byte);
    while (OBD_UART_RX_NO_DATA == rxStatus)
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
        rxStatus = LIBOBD_ReceiveByte(self, byte);
    }
    status.timeout = OBD_STATUS_OK;

    timeEnd = LIBOBD_GetTimeMs(self);
    lTimeElapsed = timeEnd - timeStart;

    // The byte arrived but was corrupted on the line
    status.response = DL_UartRxStatusToResponse(rxStatus);
    OBD2_IF_COND_GOTO_EXIT(OBD_STATUS_OK != status.response);

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

/******************************************
* title: DL_RecvByteBlocking
* brief: Waits up to MAX_BLOCKING_RECV_TIME for one byte. Returns true only
*        for a byte received without a UART or timer error.
******************************************/
static inline bool DL_RecvByteBlocking(dataLink_if_t *self, uint8_t *byte)
{
    obd_uart_rx_status_t rxStatus = OBD_UART_RX_NO_DATA;

    OBD2_IF_COND_GOTO_EXIT(OBD_TMR_OK != LIBOBD_StartTimeout(self, MAX_BLOCKING_RECV_TIME));

    while (OBD_UART_RX_NO_DATA == rxStatus)
    {
        if (LIBOBD_IsTimeoutExpired(self))
        {
            goto exit;
        }

        rxStatus = LIBOBD_ReceiveByte(self, byte);
    }

exit:
    return (OBD_UART_RX_OK == rxStatus);
}
/******************************************
* title: DL_IsTimeoutMinDone
* brief: True once minTimeout has passed since the last DL_StartTimeout.
******************************************/
static inline bool DL_IsTimeoutMinDone(dataLink_if_t *self, uint32_t minTimeout)
{
    uint32_t p2TimeElapsed = 0;

    p2TimeElapsed = LIBOBD_GetTimeMs(self);
    p2TimeElapsed -= LIBOBD_GetTimeSample(self);

    return p2TimeElapsed >= minTimeout;
}

/******************************************
* title: DL_StartTimeout
* brief: Remembers the current time as the window start and arms the timeout.
*        Reports OBD_ERR_TMR_START_FAILED when the timer cannot start.
******************************************/
static inline obd_status_t DL_StartTimeout(dataLink_if_t *self, uint32_t timeout)
{
    obd_status_t status = {0};

    LIBOBD_SetTimeSample(self, LIBOBD_GetTimeMs(self));

    if (OBD_TMR_OK != LIBOBD_StartTimeout(self, timeout))
    {
        status.response = OBD_ERR_TMR_START_FAILED;
    }

    return status;
}

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* DATA_LINK_UTILS_H */

