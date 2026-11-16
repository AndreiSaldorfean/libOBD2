#ifndef DATA_LINK_H
#define DATA_LINK_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include "timing_if.h"
#include "uart_if.h"
#include <stddef.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
// Feature flags: define LIBOBD2_SPT_<PROTOCOL> before including to pick the
// protocols. With none defined, ISO 9141-2 is built.
#if !defined(LIBOBD2_SPT_ISO9141) && !defined(LIBOBD2_SPT_KWP2000)
#define LIBOBD2_SPT_ISO9141
#endif
// LIBOBD2_SPT_KWP2000: not supported yet

// Timing limits in ms. LIBOBD2_STRICT_TIMING selects the values from the
// standard; by default the maximum limits are relaxed to 5 s. Each limit can be
// overridden on its own by defining it before including.
#if defined(LIBOBD2_STRICT_TIMING)
#define LIBOBD2_P1_MAX_DEFAULT      (20U)
#define LIBOBD2_P2_MAX_DEFAULT      (50U)
#define LIBOBD2_P2_STAR_MIN_DEFAULT (25U)
#define LIBOBD2_P4_MAX_DEFAULT      (20U)
#else
#define LIBOBD2_P1_MAX_DEFAULT      (5000U)
#define LIBOBD2_P2_MAX_DEFAULT      (5000U)
#define LIBOBD2_P2_STAR_MIN_DEFAULT (0U)
#define LIBOBD2_P4_MAX_DEFAULT      (5000U)
#endif /* LIBOBD2_STRICT_TIMING */

#ifndef P1_TIME_MIN
#define P1_TIME_MIN      (0U)
#endif
#ifndef P1_TIME_MAX
#define P1_TIME_MAX      LIBOBD2_P1_MAX_DEFAULT
#endif
#ifndef P2_TIME_MIN
#define P2_TIME_MIN      (25U)
#endif
#ifndef P2_TIME_MAX
#define P2_TIME_MAX      LIBOBD2_P2_MAX_DEFAULT
#endif
#ifndef P2_STAR_TIME_MIN
#define P2_STAR_TIME_MIN LIBOBD2_P2_STAR_MIN_DEFAULT
#endif
#ifndef P2_STAR_TIME_MAX
#define P2_STAR_TIME_MAX (5000U)
#endif
#ifndef P3_TIME_MIN
#define P3_TIME_MIN      (55U)
#endif
#ifndef P3_TIME_MAX
#define P3_TIME_MAX      (5000U)
#endif
#ifndef P4_TIME_MIN
#define P4_TIME_MIN      (5U)
#endif
#ifndef P4_TIME_MAX
#define P4_TIME_MAX      LIBOBD2_P4_MAX_DEFAULT
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
    uint32_t timeSample; /* start of the running P2 window, set by DL_StartTimeout */

    /* Protocol-specific context (e.g., kwp_ctx_t) */
    void *pProtocolCtx;

    /* Data link operations */
    dl_connect_t connect;
    dl_send_request_t send_request;
    dl_recv_response_t recv_response;
};

enum
{
#ifdef LIBOBD2_SPT_ISO9141
    ISO9141,
#endif /* LIBOBD2_SPT_ISO9141 */
#ifdef LIBOBD2_SPT_KWP2000
    KWP2000,
#endif /* LIBOBD2_SPT_KWP2000 */
    NUM_SPT_PROTOCOLS
};
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t DL_Connect(dataLink_if_t *pDataLink);
obd_status_t DL_SendRequest(dataLink_if_t *pDataLink, const obd_data_t *serviceRequests, size_t dataLen);
obd_status_t DL_RecvResponse(dataLink_if_t  *handle, obd_data_t *resp, size_t* len);
#endif /* DATA_LINK_H */
