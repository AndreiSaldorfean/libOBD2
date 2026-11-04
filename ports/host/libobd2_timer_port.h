#ifndef KWP_TIMER_H
#define KWP_TIMER_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "timing_if.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* Timer operation a fault hook is called for */
typedef enum
{
    VIRT_TMR_OP_INIT = 0U,         /* status: returned to the caller */
    VIRT_TMR_OP_GET_TIME,          /* value: time returned */
    VIRT_TMR_OP_DELAY,             /* value: delay applied */
    VIRT_TMR_OP_START_TIMEOUT,     /* value: timeout armed; status: returned to the caller */
    VIRT_TMR_OP_STOP_TIMEOUT,      /* status: returned to the caller */
    VIRT_TMR_OP_IS_TIMEOUT_EXPIRED /* expired: returned to the caller */
}virtTmrOp_t;

/* What an operation is about to do; a fault hook may change value, status and
 * expired. An operation whose status is set to an error is not carried out. */
typedef struct
{
    virtTmrOp_t op;
    uint32_t value;
    obd_tmr_status_t status;
    bool expired;
}virtTmrEvent_t;

/* Error-injection seam. A NULL hook injects nothing. */
typedef struct
{
    void (*onOp)(void *pUserData, virtTmrEvent_t *event);
    void *pUserData;
}virtTmrFaultHooks_t;

typedef struct
{
    uint32_t timeNow;
    uint32_t timeStep;
    uint32_t timeoutTime;
    bool timeoutStarted;
    virtTmrFaultHooks_t faults;
}virtTmrCtx_t;

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
