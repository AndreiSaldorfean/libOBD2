#ifndef DRIVER_TEST_VECTORS_001_H
#define DRIVER_TEST_VECTORS_001_H

/* ================================================ INCLUDES =============================================== */
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_if.h"
#include <string.h>
#include "stdint.h"
#include "target_test_fixtures.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
static virtTmrCtx_t tmr_ctx_tester =
{
    .timeNow        = 0,
    .timeStep       = 10,
    .timeoutTime    = 0,
    .timeoutStarted = 0
};

static virtTmrCtx_t tmr_ctx_ecu =
{
    .timeNow        = 0,
    .timeStep       = 10,
    .timeoutTime    = 0,
    .timeoutStarted = 0
};

static virtUartLinkCtx_t uart_ctx_tester =
{
    .tmrHandle      = &tmr_ctx_tester
};

static virtUartLinkCtx_t uart_ctx_ecu =
{
    .tmrHandle      = &tmr_ctx_ecu
};

/****************************** OPS & CONFIGS ******************************/
static obd_timing_ops_t tmr_ops =
{
        .timer_init         = LIBOBD2_TMR_Init,
        .delay_ms           = LIBOBD2_TMR_DelayMs,
        .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
        .start_timeout      = LIBOBD2_TMR_StartTimeout,
        .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

static obd_uart_ops_t uart_ops =
{
    .init        = LIBOBD2_UART_Init,
    .send_byte   = LIBOBD2_UART_WriteByte,
    .recv_byte   = LIBOBD2_UART_RecvByte,
    .send_pulse  = LIBOBD2_UART_SendPulse,
    .switch_mode = LIBOBD2_UART_SwitchMode,
    .flush_rx    = LIBOBD2_UART_FlushRx,
};

#endif /* DRIVER_TEST_VECTORS_001_H */


