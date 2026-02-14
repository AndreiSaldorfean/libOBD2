#ifndef KWP2000_MSG_H
#define KWP2000_MSG_H

/* ================================================ INCLUDES =============================================== */
#include "data_link_if.h"
#include "srv_status.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef union
{
    uint8_t val;
    struct
    {
        uint8_t l0 : 1;
        uint8_t l1 : 1;
        uint8_t l2 : 1;
        uint8_t l3 : 1;
        uint8_t l4 : 1;
        uint8_t l5 : 1;
        uint8_t a0 : 1;
        uint8_t a1 : 1;
    } bit_t;
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

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* KWP2000_MSG_H */
