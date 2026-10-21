#ifndef DATALINK_TEST_VECTORS_000_H
#define DATALINK_TEST_VECTORS_000_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "l2_iso9141.h"
#include "ecu_l2_iso9141.h"
#ifndef PLATFORM_HOST
#include "driver_test_vectors_000.h"
#else
#include "driver_test_vectors_001.h"
#endif
/* ================================================= MACROS ================================================ */
#define MSG_00_SIZE        (0x6U)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
static const header_t header_tester =
{
    .fmt     = 0x68,
    .trgAddr = 0x6A,
    .srcAddr = 0xF1,
};

static const header_t header_ecu =
{
    .fmt     = 0x48,
    .trgAddr = 0x6B,
    .srcAddr = 0x12,
};

static const obd_data_t data_tester =
{
    .sid   = 0x01,
    .param = {0x05}
};

static const obd_data_t data_ecu =
{
    .sid   = 0x41,
    .param = {0x05, 0x9A}
};

static const message_t msg_ecu =
{
    .cs = 0XA5,
    .data = data_ecu,
    .header = header_ecu
};

static const message_t msg_tester =
{
    .cs = 0XC9,
    .data = data_tester,
    .header = header_tester
};

static l2_iso9141_ctx_t dl_ctx_tester =
{
    .header = header_tester
};

static l2_iso9141_ctx_t dl_ctx_ecu   =
{
    .header = header_ecu
};

/****************************** OPS & CONFIGS ******************************/
static dataLink_if_t dl_ecu =
{
    .pProtocolCtx     = &dl_ctx_ecu,
    .pTimingOps       = &tmr_ops,
    .pTimingHandle    = &tmr_ctx_ecu,
    .pUartOps         = &uart_ops,
    .pTransportHandle = &uart_ctx_ecu,
    .connect          = ecu_l2_iso9141_connect,
    .send_request     = ecu_l2_iso9141_send_request,
    .recv_response    = ecu_l2_iso9141_recv_response,
};

static dataLink_if_t dl_tester =
{
    .pProtocolCtx     = &dl_ctx_tester,
    .pTimingOps       = &tmr_ops,
    .pTimingHandle    = &tmr_ctx_tester,
    .pUartOps         = &uart_ops,
    .pTransportHandle = &uart_ctx_tester,
    .connect          = l2_iso9141_connect,
    .send_request     = l2_iso9141_send_request,
    .recv_response    = l2_iso9141_recv_response,
};

#endif /* DATALINK_TEST_VECTORS_000_H */


