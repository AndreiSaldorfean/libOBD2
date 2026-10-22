#ifndef TIMER_UTILS_H
#define TIMER_UTILS_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "utils.h"
#include <stdbool.h>

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
bool TMR_IsTimeoutMinDone(dataLink_if_t *self, uint32_t minTimeout);
void TMR_StartTimeout(dataLink_if_t *self, uint32_t timeout);

#endif /* TIMER_UTILS_H */

