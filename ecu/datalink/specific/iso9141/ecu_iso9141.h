#ifndef ECU_ISO9141_H
#define ECU_ISO9141_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "datalink.h"
#include "iso9141.h"
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t ECU_ISO9141_Connect(dataLink_if_t *self, uint8_t* protocol);
obd_status_t ECU_ISO9141_SendRequest(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t len);
obd_status_t ECU_ISO9141_RecvResponse(dataLink_if_t *self, obd_data_t *resp, size_t *len);

#endif /* ECU_ISO9141_H */
