#ifndef DATA_LINK_H
#define DATA_LINK_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "timing_if.h"
#include "uart_if.h"
#include <stddef.h>
#include <string.h>
#include "utils.h"
#if defined(SPT_TRACING)
#include "trace.h"
#endif /* SPT_TRACING */

/* ================================================= MACROS ================================================ */
// Timing
#if 0
#define P1_TIME_MIN      (0U)
#define P1_TIME_MAX      (20U)
#define P2_TIME_MIN      (25U)
#define P2_TIME_MAX      (50U)
#define P2_STAR_TIME_MIN (25U)
#define P2_STAR_TIME_MAX (5000U)
#define P3_TIME_MIN      (55U)
#define P3_TIME_MAX      (5000U)
#define P4_TIME_MIN      (5U)
#define P4_TIME_MAX      (20U)
#else
// NO TIMEOUT
#define P1_TIME_MIN      (0U)
#define P1_TIME_MAX      (5000U)
#define P2_TIME_MIN      (25U)
#define P2_TIME_MAX      (5000U)
#define P2_STAR_TIME_MIN (0U)
#define P2_STAR_TIME_MAX (5000U)
#define P3_TIME_MIN      (55U)
#define P3_TIME_MAX      (5000U)
#define P4_TIME_MIN      (5U)
#define P4_TIME_MAX      (5000U)
#endif
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t sid;
    uint8_t param[11]; // NOTE: Since only iso9141 is supported, only 7 data bytes are needed, but i left some buffer bytes so bugs will be avoided in development phase
}obd_data_t;

typedef struct header_t
{
    uint8_t fmt;
    uint8_t trgAddr;
    uint8_t srcAddr;
    uint8_t len;
} header_t;

typedef struct
{
    header_t header;
    obd_data_t data;
    uint8_t cs;
} message_t;

/* Forward declaration */
typedef struct dataLink_if dataLink_if_t;
typedef obd_status_t (*dl_connect_t)(dataLink_if_t*, uint8_t*);
typedef obd_status_t (*dl_send_request_t)(dataLink_if_t *pDataLink, const obd_data_t *serviceRequests, size_t dataLen);
typedef obd_status_t (*dl_recv_response_t)(dataLink_if_t *pDataLink, obd_data_t *resp, size_t* dataLen);

struct dataLink_if
{
    /* Uart layer */
    obd_uart_ops_t *pUartOps;
    void *pTransportHandle;

    /* Timing layer */
    obd_timing_ops_t *pTimingOps;
    void *pTimingHandle;

    /* Protocol-specific context (e.g., l2_kwp_ctx_t) */
    void *pProtocolCtx;

    /* Data link operations */
    dl_connect_t connect;
    dl_send_request_t send_request;
    dl_recv_response_t recv_response;
};

enum
{
    ISO9141,
    KWP2000
};
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t DL_Connect(dataLink_if_t *pDataLink);
obd_status_t DL_SendRequest(dataLink_if_t *pDataLink, const obd_data_t *serviceRequests, size_t dataLen);
obd_status_t DL_RecvResponse(dataLink_if_t  *handle, obd_data_t *resp, size_t* len);
#endif /* DATA_LINK_H */
