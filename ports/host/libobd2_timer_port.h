#ifndef KWP_TIMER_H
#define KWP_TIMER_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "timing_if.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint32_t timeNow;
    uint32_t timeStep;
    uint32_t timeoutTime;
    bool timeoutStarted;
}virtTmrCtx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
bool LIBOBD2_TMR_Init(void *pHandle);
uint32_t LIBOBD2_TMR_GetTimeMs(void *pHandle);
void LIBOBD2_TMR_DelayMs(void *pHandle, uint32_t delay_ms);
bool LIBOBD2_TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData);
bool LIBOBD2_TMR_StopTimeout(void *pHandle);
bool LIBOBD2_TMR_IsTimeoutExpired(void *pHandle);

#endif /* KWP_TIMER_H */
