#ifndef ECU_DATA_LINK_H
#define ECU_DATA_LINK_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "datalink.h"
#include "timing_if.h"
#include "uart_if.h"
#include <stddef.h>
#include <string.h>
#include "utils.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t ECU_DL_Connect(dataLink_if_t *pDataLink);
obd_status_t ECU_DL_SendRequest(dataLink_if_t *handle, const obd_data_t *serviceRequests, size_t len);
obd_status_t ECU_DL_RecvResponse(dataLink_if_t  *handle, obd_data_t *resp, size_t* len);

#endif /* ECU_DATA_LINK_H */
