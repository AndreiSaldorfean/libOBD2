#ifndef KWP2000_H
#define KWP2000_H

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"
#include <stdbool.h>
#include <stdint.h>
#include "data_link_if.h"
#include <stddef.h>

/* ================================================= MACROS ================================================ */
#define SPT_5BAUD_INIT
// #define SPT_FAST_INIT
// #define SPT_CHANGE_TIMING_PARAM
#define FAST_INIT_WAKEUP_START  (0x1)
#define FAST_INIT_WAKEUP_END    (0x2)
#define SLOW_INIT_5BAUD_START   (0x3)
#define SLOW_INIT_5BAUD_END     (0x4)

// Timing
#define KWP_P1_TIME_MIN      (0U)
#define KWP_P1_TIME_MAX      (20U)
#define KWP_P2_TIME_MIN      (25U)
#define KWP_P2_TIME_MAX      (50U)
#define KWP_P2_STAR_TIME_MIN (25U)
#define KWP_P2_STAR_TIME_MAX (5000U)
#define KWP_P3_TIME_MIN      (55U)
#define KWP_P3_TIME_MAX      (5000U)
#define KWP_P4_TIME_MIN      (5U)
#define KWP_P4_TIME_MAX      (20U)

#define PULSE_HIGH (1U)
#define PULSE_LOW (0U)
/********************************************* SERVICES SECTION **********************************************/
#define START_COMM_REQ \
    ((data_t){ \
        .req = \
        { \
            .sid   = 0x81, \
            .param = {0x0}, \
        },\
        .len   = 0x0, \
    })
/********************************************* SERVICES SECTION **********************************************/

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/********************************************* MESSAGES SECTION **********************************************/
typedef union
{
    uint8_t val;
    struct
    {
        uint8_t len : 6;
        uint8_t a0  : 1;
        uint8_t a1  : 1;
    } bit;
} format_t;

typedef struct
{
    union
    {
        obd_request_t req;
        obd_response_t resp;
    };
    size_t len;
}data_t;

typedef struct
{
    format_t fmt;
    uint8_t trgAddr;
    uint8_t srcAddr;
    uint8_t len;
} header_t;

typedef struct
{
    header_t header;
    data_t data;
    uint8_t cs;
} message_t;

/********************************************* MESSAGES SECTION **********************************************/

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
