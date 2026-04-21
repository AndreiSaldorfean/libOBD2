#ifndef LIBOBD2_TEST_UTILS_H
#define LIBOBD2_TEST_UTILS_H
/* ================================================ INCLUDES =============================================== */
#include "l2_kwp.h"
#include "kwp_timer.h"
#include "uart_kwp_transport_port.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
extern const header_t header_00_ecu;
extern const header_t header_00;
extern const obd_request_t request_00;
extern const data_t data_00;
extern const message_t msg_00;
extern const message_t msg_00_ecu;
extern timerCtx_t tmrCtxTx;
extern timerCtx_t tmrCtxRx;
extern obd_timing_ops_t timerOpsRx;
extern obd_timing_ops_t timerOpsTx;
extern uartKwp_ctx_t uartCtxTx;
// extern timerCtx_t tmrCtxTx;
extern uartKwp_ctx_t uartCtxRx;
// extern timerCtx_t tmrCtxRx;
extern obd_transport_ops_t transportOps;
extern l2_kwp_ctx_t kwpCtx;
extern dataLink_if_t dataLink_00;
extern dataLink_if_t dataLink_tx;
extern dataLink_if_t dataLink_rx;

#if 0
static obd_ctx_t ctx;
#endif
/* =============================================== MODULE API ============================================== */

#endif /* LIBOBD2_TEST_UTILS_H */
