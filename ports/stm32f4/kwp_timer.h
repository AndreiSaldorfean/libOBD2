#ifndef KWP_TIMER_H
#define KWP_TIMER_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "timing_if.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    volatile uint32_t  timeout_start_ms;     /* Timestamp when timeout started */
    volatile uint32_t  timeout_duration_ms;  /* Timeout duration for wrap-around safe comparison */
    timing_callback_t  timeout_callback;
    void              *timeout_user_data;
    volatile bool      timeout_active;
    volatile bool      timeout_expired;

    uint32_t timerClk;
    uint32_t rstTimer;
    uint32_t timer;
    uint32_t timerPrescaler;
    uint32_t event;
    uint32_t flag;
}timerCtx_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
bool KWP_TMR_Init(void *pHandle);
uint32_t KWP_TMR_GetTimeMs(void *pHandle);
void KWP_TMR_DelayMs(void *pHandle, uint32_t delay_ms);
bool KWP_TMR_StartTimeout(void *pHandle, uint32_t timeout_ms, timing_callback_t callback, void *pUserData);
bool KWP_TMR_StopTimeout(void *pHandle);
bool KWP_TMR_IsTimeoutExpired(void *pHandle);

/* Debug helpers – freeze/resume the logical clock at GDB breakpoints */
void KWP_TMR_Pause(void);
void KWP_TMR_Resume(void);

#endif /* KWP_TIMER_H */
