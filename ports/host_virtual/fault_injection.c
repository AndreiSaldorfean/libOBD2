/* ================================================ INCLUDES =============================================== */
#include "fault_injection.h"
#include "uart_trace.h"
#include "utils.h"
#include <stddef.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static inline bool FAULT_IsHit(uint32_t idx, uint32_t first, uint32_t count);
static bool FAULT_ApplyUartRule(const faultUartRule_t *rule, uint32_t idx, uartEvent_t *event);
static void FAULT_OnUartWrite(void *pUserData, uartEvent_t *event);
static void FAULT_OnUartRead(void *pUserData, uartEvent_t *event);
static void FAULT_ApplyTmrRule(const faultTmrRule_t *rule, virtTmrEvent_t *event);
static void FAULT_OnTmrOp(void *pUserData, virtTmrEvent_t *event);

static bool FAULT_TmrAttach_Validate(const virtTmrCtx_t *tmr, const faultTmrInjector_t *injector);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: FAULT_IsHit
* brief: True when the idx-th byte/call falls in the window [first, first + count),
*        or anywhere from first on when count is 0.
******************************************/
static inline bool FAULT_IsHit(uint32_t idx, uint32_t first, uint32_t count)
{
    return (idx >= first) && ((0U == count) || ((idx - first) < count));
}

/******************************************
* title: FAULT_ApplyUartRule
* brief: Applies rule to the idx-th byte of its direction; true if it changed
*        the byte or its error.
******************************************/
static bool FAULT_ApplyUartRule(const faultUartRule_t *rule, uint32_t idx, uartEvent_t *event)
{
    bool hit = false;

    if (((OBD_UART_RX_OK != rule->error) || (0U != rule->xorMask)) && FAULT_IsHit(idx, rule->firstByte, rule->count))
    {
        event->byte ^= rule->xorMask;
        if (OBD_UART_RX_OK != rule->error)
        {
            event->error = rule->error;
        }
        hit = true;
    }

    return hit;
}

/******************************************
* title: FAULT_OnUartWrite
* brief: onWrite hook: counts a byte put on the line and applies the onWrite rule.
******************************************/
static void FAULT_OnUartWrite(void *pUserData, uartEvent_t *event)
{
    faultUartInjector_t *injector = (faultUartInjector_t *)pUserData;

    if (FAULT_ApplyUartRule(&injector->onWrite, injector->written, event))
    {
        injector->injected++;
        UART_TRACE_OnFault(event->timeReceived, "uart_write_fault");
    }
    injector->written++;
}

/******************************************
* title: FAULT_OnUartRead
* brief: onRead hook: counts a byte handed to a receiver and applies the onRead rule.
******************************************/
static void FAULT_OnUartRead(void *pUserData, uartEvent_t *event)
{
    faultUartInjector_t *injector = (faultUartInjector_t *)pUserData;

    if (FAULT_ApplyUartRule(&injector->onRead, injector->read, event))
    {
        injector->injected++;
        UART_TRACE_OnFault(injector->link->tmrHandle->timeNow, "uart_read_fault");
    }
    injector->read++;
}

/******************************************
* title: FAULT_ApplyTmrRule
* brief: Applies the rule's effect to a timer operation that is in its window.
******************************************/
static void FAULT_ApplyTmrRule(const faultTmrRule_t *rule, virtTmrEvent_t *event)
{
    int64_t shifted = 0;

    switch (rule->effect)
    {
        case FAULT_TMR_FAIL:
            event->status = OBD_TMR_ERR_HW;
            break;

        case FAULT_TMR_EXPIRE:
            event->expired = true;
            break;

        default: /* FAULT_TMR_SHIFT, the only other effect FAULT_TmrAttach accepts */
            shifted      = (int64_t)event->value + rule->shiftMs;
            event->value = (shifted < 0) ? 0U : (uint32_t)shifted;
            break;
    }
}

/******************************************
* title: FAULT_OnTmrOp
* brief: onOp hook: counts the call and applies the rule when it hits this call
*        of its operation.
******************************************/
static void FAULT_OnTmrOp(void *pUserData, virtTmrEvent_t *event)
{
    faultTmrInjector_t *injector = (faultTmrInjector_t *)pUserData;
    const faultTmrRule_t *rule = &injector->rule;
    uint32_t idx = injector->calls[event->op]++;

    if ((event->op == rule->op) && FAULT_IsHit(idx, rule->firstCall, rule->count))
    {
        FAULT_ApplyTmrRule(rule, event);
        injector->injected++;
        UART_TRACE_OnFault(injector->tmr->timeNow, "tmr_fault");
    }
}

/******************************************
* title: FAULT_TmrAttach_Validate
* brief: Checks the arguments of FAULT_TmrAttach: the rule's effect must fit
*        its operation.
******************************************/
static bool FAULT_TmrAttach_Validate(const virtTmrCtx_t *tmr, const faultTmrInjector_t *injector)
{
    bool ok = false;

    if ((NULL != tmr) && (NULL != injector))
    {
        switch (injector->rule.effect)
        {
            case FAULT_TMR_FAIL:
                ok = (VIRT_TMR_OP_INIT == injector->rule.op) || (VIRT_TMR_OP_START_TIMEOUT == injector->rule.op)
                  || (VIRT_TMR_OP_STOP_TIMEOUT == injector->rule.op);
                break;

            case FAULT_TMR_EXPIRE:
                ok = (VIRT_TMR_OP_IS_TIMEOUT_EXPIRED == injector->rule.op);
                break;

            case FAULT_TMR_SHIFT:
                ok = (VIRT_TMR_OP_GET_TIME == injector->rule.op) || (VIRT_TMR_OP_DELAY == injector->rule.op);
                break;

            default:
                break;
        }
    }

    return ok;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: FAULT_UartAttach
* brief: Resets the injector's counters and installs it on the link's fault
*        hooks. False for a NULL link or injector.
******************************************/
bool FAULT_UartAttach(virtUartLinkCtx_t *link, faultUartInjector_t *injector)
{
    bool ok = (NULL != link) && (NULL != injector);

    if (ok)
    {
        injector->written  = 0U;
        injector->read     = 0U;
        injector->injected = 0U;
        injector->link     = link;

        link->faults.onWrite   = FAULT_OnUartWrite;
        link->faults.onRead    = FAULT_OnUartRead;
        link->faults.pUserData = injector;
    }

    return ok;
}

/******************************************
* title: FAULT_UartDetach
* brief: Removes any fault hooks from the link.
******************************************/
void FAULT_UartDetach(virtUartLinkCtx_t *link)
{
    OBD2_DEBUG_ASSERT(NULL != link);

    link->faults = (virtUartFaultHooks_t){0};
}

/******************************************
* title: FAULT_TmrAttach
* brief: Resets the injector's counters and installs it on the timer's fault
*        hook. False for NULL arguments or an effect that does not fit the op.
******************************************/
bool FAULT_TmrAttach(virtTmrCtx_t *tmr, faultTmrInjector_t *injector)
{
    bool ok = FAULT_TmrAttach_Validate(tmr, injector);

    if (ok)
    {
        (void)memset(injector->calls, 0, sizeof(injector->calls));
        injector->injected = 0U;
        injector->tmr      = tmr;

        tmr->faults.onOp      = FAULT_OnTmrOp;
        tmr->faults.pUserData = injector;
    }

    return ok;
}

/******************************************
* title: FAULT_TmrDetach
* brief: Removes any fault hook from the timer.
******************************************/
void FAULT_TmrDetach(virtTmrCtx_t *tmr)
{
    OBD2_DEBUG_ASSERT(NULL != tmr);

    tmr->faults = (virtTmrFaultHooks_t){0};
}
