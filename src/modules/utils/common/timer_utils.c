/* ================================================ INCLUDES =============================================== */
#include "timer_utils.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
bool TMR_IsTimeoutMinDone(dataLink_if_t *self, uint32_t minTimeout)
{
    uint32_t p2TimeElapsed = 0;

    p2TimeElapsed = LIBOBD_GetTimeMs(self);
    p2TimeElapsed -= LIBOBD_GetTimeSample(self);

    return p2TimeElapsed >= minTimeout;
}

void TMR_StartTimeout(dataLink_if_t *self, uint32_t timeout)
{
    LIBOBD_SetTimeSample(self, LIBOBD_GetTimeMs(self));
    LIBOBD_StartTimeout(self, timeout);
}
