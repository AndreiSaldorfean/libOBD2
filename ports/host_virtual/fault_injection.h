#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

/*************************************************************************************************************
 * Error injection for the host virtual UART and timer ports.
 *
 * A test fills in a rule, attaches an injector to a link (UART) or timer context, and runs the code under
 * test. The injector uses the ports' fault hooks (virtUartFaultHooks_t, virtTmrFaultHooks_t), counts the
 * bytes / calls it sees and applies the rule to the ones in its window. Every applied fault is counted in
 * 'injected' and shown as a pulse on kline.fault in the VCD trace.
 *
 *   faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_FRAMING_ERR, .firstByte = 3, .count = 1 } };
 *   FAULT_UartAttach(&uartCtx, &inj);      // the 4th byte put on the line arrives with a framing error
 *
 * Injectors keep working across UART_Init / TMR_Init; FAULT_*Detach removes them.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include <stdbool.h>
#include <stdint.h>
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_if.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* Which bytes of one direction get which fault */
typedef struct
{
    obd_uart_rx_status_t error;   /* error the receiver reports for a hit byte (OBD_UART_RX_OK: none) */
    uint8_t  xorMask;             /* bits flipped in a hit byte (0: byte unchanged) */
    uint32_t firstByte;           /* first hit byte, counted from 0 after attach */
    uint32_t count;               /* hit bytes from firstByte on; 0 = all of them */
} faultUartRule_t;

typedef struct
{
    faultUartRule_t onWrite;      /* bytes put on the line: both ends of the link see the fault */
    faultUartRule_t onRead;       /* bytes handed to a receiver, e.g. an overrun */

    /* Filled in by the injector */
    uint32_t written;             /* bytes put on the line since attach */
    uint32_t read;                /* bytes handed to a receiver since attach */
    uint32_t injected;            /* faults applied */
    const virtUartLinkCtx_t *link;
} faultUartInjector_t;

/* What a timer fault does; each effect fits only some operations */
typedef enum
{
    FAULT_TMR_FAIL = 1U,          /* INIT / START_TIMEOUT / STOP_TIMEOUT return OBD_TMR_ERR_HW */
    FAULT_TMR_EXPIRE,             /* IS_TIMEOUT_EXPIRED returns true */
    FAULT_TMR_SHIFT,              /* GET_TIME returns, or DELAY waits, shiftMs more (clamped at 0) */
} faultTmrEffect_t;

typedef struct
{
    virtTmrOp_t op;               /* operation the fault hits */
    faultTmrEffect_t effect;
    int32_t  shiftMs;             /* FAULT_TMR_SHIFT only */
    uint32_t firstCall;           /* first hit call of 'op', counted from 0 after attach */
    uint32_t count;               /* hit calls from firstCall on; 0 = all of them */
} faultTmrRule_t;

typedef struct
{
    faultTmrRule_t rule;

    /* Filled in by the injector */
    uint32_t calls[VIRT_TMR_OP_COUNT]; /* calls of each operation since attach */
    uint32_t injected;                 /* faults applied */
    const virtTmrCtx_t *tmr;
} faultTmrInjector_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
bool FAULT_UartAttach(virtUartLinkCtx_t *link, faultUartInjector_t *injector);
void FAULT_UartDetach(virtUartLinkCtx_t *link);
bool FAULT_TmrAttach(virtTmrCtx_t *tmr, faultTmrInjector_t *injector);
void FAULT_TmrDetach(virtTmrCtx_t *tmr);

#endif /* FAULT_INJECTION_H */
