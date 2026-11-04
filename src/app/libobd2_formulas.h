#ifndef LIBOBD2_FORMULAS_H
#define LIBOBD2_FORMULAS_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "libobd2_service_requests.h"
#include "utils.h"
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define LIBOBD2_MULTIFRAME_DATA_IDX  (2U)  /* param[] index of the first data byte, after PID and message number */
#define LIBOBD2_COOLANT_TEMP_IDX     (1U)  /* param[] index of A in a $01 $05 response */
#define LIBOBD2_COOLANT_TEMP_OFFSET  (40)  /* coolant temperature = A - 40 degC */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/******************************************
* title: LibOBD2_FormulaMultiFrame
* brief: Joins the data bytes of a $09 multi-message answer (VIN, CALID, ...)
*        into out and returns how many bytes it wrote.
******************************************/
static inline size_t LibOBD2_FormulaMultiFrame(const libobd2_data_t *msgs, size_t msgCnt, uint8_t *out)
{
    size_t outLen   = 0;
    size_t paramCnt = 0;

    OBD2_DEBUG_ASSERT((NULL != msgs) && (NULL != out));

    for (size_t msg = 0; msg < msgCnt; msg++)
    {
        /* dataLen counts the sid, the rest are params */
        paramCnt = (0U < msgs[msg].dataLen) ? (msgs[msg].dataLen - 1U) : 0U;

        for (size_t param = LIBOBD2_MULTIFRAME_DATA_IDX; param < paramCnt; param++)
        {
            out[outLen++] = msgs[msg].data.param[param];
        }
    }

    return outLen;
}

/******************************************
* title: LibOBD2_FormulaCoolantTemp
* brief: Engine coolant temperature ($01 $05) in degC: A - 40, as one byte.
******************************************/
static inline uint8_t LibOBD2_FormulaCoolantTemp(const libobd2_data_t *msg)
{
    OBD2_DEBUG_ASSERT(NULL != msg);

    return (uint8_t)(msg->data.param[LIBOBD2_COOLANT_TEMP_IDX] - LIBOBD2_COOLANT_TEMP_OFFSET);
}

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_FORMULAS_H */
