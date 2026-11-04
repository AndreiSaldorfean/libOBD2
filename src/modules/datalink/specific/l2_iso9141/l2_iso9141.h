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
    uint8_t protocol;
    uint8_t kb1;
    uint8_t kb2;
}l2_iso9141_ctx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
/******************************************
* title: l2_iso9141_connect
* brief: Opens an ISO9141-2 connection to the ECU (5-baud init) and stores
*        the header to use in the protocol context.
* input: self     - data link, must have a protocol context
*        protocol - output for the selected protocol
* output: obd_status_t - OK, OBD_ERR_NULL_PTR, OBD_ERR_PROTOCOL_NOT_FOUND,
*         or the OBD_ERR_5BAUD_* code of the failed init step
******************************************/
obd_status_t l2_iso9141_connect(dataLink_if_t *self, uint8_t* protocol);

/******************************************
* title: l2_iso9141_send_request
* brief: Sends an OBD service request to the ECU over ISO9141-2.
* input: self            - connected data link, must have a protocol context
*        serviceRequests - sid and params to send
*        dataLen         - sid + params count, 1..7 (sid + up to 6 params)
* output: obd_status_t - OK, OBD_ERR_NULL_PTR, OBD_ERR_INVALID_PARAM,
*         or the error of the failed send step
******************************************/
obd_status_t l2_iso9141_send_request(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);

/******************************************
* title: l2_iso9141_recv_response
* brief: Receives the ECU response to the last request over ISO9141-2.
* input: self    - data link, after l2_iso9141_send_request
*        resp    - output for the response data (sid + params)
*        dataLen - output for the number of data bytes
* output: obd_status_t - OK, OBD_ERR_NULL_PTR, OBD_ERR_COMM_ECU_RESPONSE_7F,
*         or the error of the failed receive step
******************************************/
obd_status_t l2_iso9141_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen);

#endif /* L2_ISO9141_H */
