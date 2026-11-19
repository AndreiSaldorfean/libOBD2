/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "datalink.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <stddef.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC obd_status_t LibOBD2_Init_Validate(obd_ctx_t *ctx);
OBD2_STATIC obd_status_t LibOBD2_Init_Execute(obd_ctx_t *ctx);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: LibOBD2_Init_Validate
* brief: Checks the arguments of LibOBD2_Init.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_Init_Validate(obd_ctx_t *ctx)
{
    obd_status_t status = {0};

    if ((NULL == ctx) || (NULL == ctx->pDataLink))
    {
        status.response = OBD_ERR_NULL_PTR;
    }

    return status;
}

/******************************************
* title: LibOBD2_Init_Execute
* brief: Connects the data link (protocol detection and init).
******************************************/
OBD2_STATIC obd_status_t LibOBD2_Init_Execute(obd_ctx_t *ctx)
{
    return DL_Connect(ctx->pDataLink);
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: LibOBD2_Init
* brief: Connects to the ECU over the context's data link.
******************************************/
obd_status_t LibOBD2_Init(obd_ctx_t *ctx)
{
    obd_status_t status = {0};

    status = LibOBD2_Init_Validate(ctx);
    OBD2_ASSERT_OK(status);
    status = LibOBD2_Init_Execute(ctx);

exit:
    return status;
}
