#ifndef KWP2000_H
#define KWP2000_H

/* ================================================ INCLUDES =============================================== */
#include "l2_kwp_msg.h"
#include "srv_status.h"
#include <stdbool.h>
#include <stdint.h>
#include "data_link_if.h"
#include <stddef.h>

/* ================================================= MACROS ================================================ */
// #define SPT_5BAUD_INIT
// #define SPT_CHANGE_TIMING_PARAM
#define FAST_INIT_WAKEUP_START (0x1)
#define FAST_INIT_WAKEUP_END   (0x2)
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
obd_status_t l2_kwp_connect(dataLink_if_t *pDataLink);
obd_status_t l2_kwp_send_request(dataLink_if_t *self, const obd_request_t *req, size_t len);
obd_status_t l2_kwp_recv_response(dataLink_if_t *self, obd_response_t *resp, size_t* len);

#endif /* KWP2000_H */
