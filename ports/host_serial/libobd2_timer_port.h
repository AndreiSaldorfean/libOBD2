#ifndef KWP_TIMER_H
#define KWP_TIMER_H

/*************************************************************************************************************
 * Timer port for x86 hosts (POSIX): CLOCK_MONOTONIC time base, clock_nanosleep delays, polled timeouts.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>
#include <stdbool.h>
#include "statusRetCodes.h"
#include "timing_if.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint64_t originMs;             /* monotonic time at TMR_Init; TMR_GetTimeMs counts from here */
    uint32_t timeoutStartMs;
    uint32_t timeoutDurationMs;
    timing_callback_t timeoutCallback;
    void *timeoutUserData;
    bool timeoutActive;
    bool timeoutExpired;
}serialTmrCtx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_tmr_status_t TMR_Init(void *pHandle);
uint32_t TMR_GetTimeMs(void *pHandle);
void TMR_DelayMs(void *pHandle, uint32_t delay_ms);
obd_tmr_status_t TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData);
obd_tmr_status_t TMR_StopTimeout(void *pHandle);
bool TMR_IsTimeoutExpired(void *pHandle);

#endif /* KWP_TIMER_H */
