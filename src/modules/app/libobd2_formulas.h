#ifndef LIBOBD2_FORMULAS_H
#define LIBOBD2_FORMULAS_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "libobd2_service_requests.h"
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define OP_VIN(d, libobd2DataLen, resp, respLen) \
    do { \
        size_t cnt = 0; \
        for (size_t i = 0; i < libobd2DataLen; i++) \
        { \
            for (size_t j = 3; j < d[i].dataLen - 1; j++) \
            { \
                resp[cnt++] = d[i].data.param[j]; \
            } \
        } \
        *respLen = cnt; \
    }while(0)

#define OP_CALID(d, libobd2DataLen, resp, respLen) \
    do { \
        size_t cnt = 0; \
        for (size_t i = 0; i < libobd2DataLen; i++) \
        { \
            for (size_t j = 3; j < d[i].dataLen - 1; j++) \
            { \
                resp[cnt++] = d[i].data.param[j]; \
            } \
        } \
        *respLen = cnt; \
    }while(0)

#define OP_COOLANT_TEMP(d, resp) \
    do { \
        resp[0] = d[0].data.param[1] - 40; \
    }while(0)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_FORMULAS_H */
