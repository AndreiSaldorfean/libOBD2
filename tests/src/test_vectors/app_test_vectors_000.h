#ifndef APP_TEST_VECTORS_000_H
#define APP_TEST_VECTORS_000_H

/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "datalink_test_vectors_000.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_ctx_t ctx_ecu =
{
        .pDataLink = &dl_ecu,
        // .pDataLinkHandle = &kwpCtx,
        .connectionStatus = 0
};

obd_ctx_t ctx_tester =
{
        .pDataLink = &dl_tester,
        // .pDataLinkHandle = &kwpCtx,
        .connectionStatus = 0
};


#endif /* TEST_VECTORS_000_H */

