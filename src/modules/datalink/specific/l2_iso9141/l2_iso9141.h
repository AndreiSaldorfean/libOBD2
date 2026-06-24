#ifndef L2_ISO9141_H
#define L2_ISO9141_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "datalink.h"
/* ================================================= MACROS ================================================ */

#define ISO9141_W0_TIME_MIN (2U)
#define ISO9141_W1_TIME_MIN (60U)
#define ISO9141_W1_TIME_MAX (300U)
#define ISO9141_W2_TIME_MIN (5U)
#define ISO9141_W2_TIME_MAX (20U)
#define ISO9141_W3_TIME_MIN (0U)
#define ISO9141_W3_TIME_MAX (20U)
#define ISO9141_W4_TIME_MIN (25U)
#define ISO9141_W4_TIME_MAX (50U)
#define ISO9141_W4_INV_ADDR_TIME_MAX (200U)
#define ISO9141_W5_TIME_MIN (300U)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    header_t header;
    uint8_t kb1;
    uint8_t kb2;
}l2_iso9141_ctx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t l2_iso9141_connect(dataLink_if_t *self, uint8_t* protocol);
obd_status_t l2_iso9141_send_request(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);
obd_status_t l2_iso9141_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen);

#endif /* L2_ISO9141_H */
