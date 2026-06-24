#ifndef L2_KWP2000_H
#define L2_KWP2000_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include <stdbool.h>
#include <stdint.h>
#include "datalink.h"
#include <stddef.h>
#include "string.h"

/* ================================================= MACROS ================================================ */
/********************************************* SERVICES SECTION **********************************************/
#define START_COMM_REQ \
    ((obd_data_t){ \
        .serviceRequests = \
        { \
            .sid   = 0x81, \
            .param = {0x0}, \
        },\
        .len   = 0x0, \
    })
/********************************************* SERVICES SECTION **********************************************/

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef union
{
    uint8_t val;
    struct
    {
        uint8_t POR        : 1;
        uint8_t STOP_COMM  : 1;
        uint8_t P3_TIMEOUT : 1;
        uint8_t Reserved   : 4;
        uint8_t CONN_OK    : 1;
    }bits;
}connectionStatus_t;

typedef struct
{
    connectionStatus_t conStatus;
    header_t header;
    uint8_t kb1;
    uint8_t kb2;
}l2_kwp_ctx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t l2_kwp_connect(dataLink_if_t *self, uint8_t* protocol);
obd_status_t l2_kwp_send_request(dataLink_if_t *self, const obd_data_t *serviceRequests, size_t dataLen);
obd_status_t l2_kwp_recv_response(dataLink_if_t *self, obd_data_t *resp, size_t* dataLen);

#endif /* L2_KWP2000_H */
