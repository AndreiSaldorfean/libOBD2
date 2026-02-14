#ifndef KWP_TIMER_H
#define KWP_TIMER_H

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"
#include "timing_if.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint32_t          timeout_target_ms;
    timing_callback_t timeout_callback;
    void             *timeout_user_data;
    bool              timeout_active;
    bool              timeout_expired;
}timerCtx_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t KWP_TMR_Init(void *pHandle);
uint32_t KWP_TMR_GetTimeMs(void *pHandle);
obd_status_t KWP_TMR_DelayMs(void *pHandle, uint32_t delay_ms);
obd_status_t KWP_TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData);
obd_status_t KWP_TMR_StopTimeout(void *pHandle);
bool KWP_TMR_IsTimeoutExpired(void *pHandle);

#endif /* KWP_TIMER_H */
