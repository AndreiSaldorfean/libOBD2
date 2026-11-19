/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "libobd2_priv.h"
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
* brief: Connects the data link (protocol detection and init) and reads
*        which sids / PIDs the ECU supports. The context only counts as
*        connected once both worked.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_Init_Execute(obd_ctx_t *ctx)
{
    obd_status_t status = {0};

    ctx->connectionStatus = false;

    status = DL_Connect(ctx->pDataLink);
    OBD2_ASSERT_OK(status);

    // Cache which of $01 $02 $05 $06 $08 $09 and which of their PIDs the ECU supports
    status = LibOBD2_ReadSupportedPids(ctx);
    OBD2_ASSERT_OK(status);

    ctx->connectionStatus = true;

exit:
    return status;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: LibOBD2_Init
* brief: Connects to the ECU over the context's data link and reads
*        which PIDs it supports.
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
