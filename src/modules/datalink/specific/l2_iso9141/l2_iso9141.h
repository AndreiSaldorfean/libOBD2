#ifndef L2_ISO9141_H
#define L2_ISO9141_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "datalink.h"
/* ================================================= MACROS ================================================ */

#define ISO9141_W0_TIME_MIN (2U)   // Time the kline shall be kept high before init
#define ISO9141_W1_TIME_MIN (60U)  // Time the kline shall be kept high before init
#define ISO9141_W1_TIME_MAX (300U) // Time the kline shall be kept high before init
#define ISO9141_W2_TIME_MIN (5U)   // Time the kline shall be kept high before init
#define ISO9141_W2_TIME_MAX (20U)  // Time the kline shall be kept high before init
#define ISO9141_W3_TIME_MIN (0U)   // Time the kline shall be kept high before init
#define ISO9141_W3_TIME_MAX (20U)  // Time the kline shall be kept high before init
#define ISO9141_W4_TIME_MIN (25U)   // Time the kline shall be kept high before init
#define ISO9141_W4_TIME_MAX (50U)   // Time the kline shall be kept high before init
#define ISO9141_W4_INV_ADDR_TIME_MAX (200U) // Relaxed: some ECUs respond up to ~150ms after ~KB2
#define ISO9141_W5_TIME_MIN (300U)  // Time the kline shall be kept high before init

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
obd_status_t l2_iso9141_send_request(dataLink_if_t *self, const obd_data_t *req, size_t dataLen);
obd_status_t l2_iso9141_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen);

#endif /* L2_ISO9141_H */
