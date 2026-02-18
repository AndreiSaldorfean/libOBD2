#ifndef KWP2000_UTILS_H
#define KWP2000_UTILS_H

/* ================================================ INCLUDES =============================================== */
#include "data_link_if.h"
#include "l2_kwp.h"
#include "srv_status.h"

/* ================================================= MACROS ================================================ */
#define MAX_BLOCKING_RECV_TIME (300U)
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
static inline void SendByteBitBang(dataLink_if_t *self, uint8_t byte, uint8_t buadRate)
{
    const uint16_t delay = (1000 / buadRate);

    // Set line HIGH (idle) and switch to bit-bang mode for 5 baud
    LIBOBD_SwitchMode(self, SLOW_INIT_5BAUD_START);

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
    LIBOBD_SwitchMode(self, SLOW_INIT_5BAUD_END);
}

static inline obd_status_t ReadByteInTimeframe(dataLink_if_t *self, uint8_t *byte, uint16_t timeMin, uint16_t timeMax)
{
    uint32_t timeStart = 0;
    uint32_t timeEnd = 0;
    uint32_t timeElapsed = 0;

    timeStart = LIBOBD_GetTimeMs(self);
    LIBOBD_StartTimeout(self, timeMax);

    while (OBD_RECV_NOT_READY == LIBOBD_ReceiveByte(self, byte))
    {
        if (LIBOBD_IsTimeoutExpired(self))
            return OBD_ERR_TIMEOUT_MAX;
    }
    timeEnd = LIBOBD_GetTimeMs(self);
    LIBOBD_StopTimeout(self);
    timeElapsed = timeEnd - timeStart;

    if (timeElapsed < timeMin || timeElapsed >= timeMax)
        return OBD_ERR_TIMEOUT_MIN;
    return OBD_STATUS_OK;
}

static inline obd_status_t RecvByteBlocking(dataLink_if_t *self, uint8_t *byte)
{
    LIBOBD_StartTimeout(self, MAX_BLOCKING_RECV_TIME);

    while (OBD_RECV_NOT_READY == LIBOBD_ReceiveByte(self, byte))
    {
        if (LIBOBD_IsTimeoutExpired(self))
            return OBD_ERR_TIMEOUT_MAX;
    }

    return OBD_STATUS_OK;
}

#endif /* KWP2000_UTILS_H */
