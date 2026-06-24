#ifndef ECU_L2_ISO9141_H
#define ECU_L2_ISO9141_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "datalink.h"
#include "l2_iso9141.h"
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t ecu_l2_iso9141_connect(dataLink_if_t *self, uint8_t* protocol);
obd_status_t ecu_l2_iso9141_send_request(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t len);
obd_status_t ecu_l2_iso9141_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t *len);

#endif /* ECU_L2_ISO9141_H */
