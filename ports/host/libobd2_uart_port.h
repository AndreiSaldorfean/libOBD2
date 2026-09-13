#ifndef UART_TRANSPORT_PORT_H
#define UART_TRANSPORT_PORT_H

/* ================================================ INCLUDES =============================================== */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "libobd2_timer_port.h"
#include "statusRetCodes.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t byte;
    uint32_t timeReceived;
    bool valid;
}uartEvent_t;

typedef struct
{
    uartEvent_t *uartEventQueue;
    size_t queueSize;
    uint32_t head;
    uint32_t tail;
    virtTmrCtx_t *tmrHandle;
}virtUartLinkCtx_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
bool LIBOBD2_UART_Init(void* handle);
bool LIBOBD2_UART_RecvByte(void* handle, uint8_t *recv_buffer);
void LIBOBD2_UART_WriteByte(void* handle, uint8_t data);
void LIBOBD2_UART_FlushRx(void* handle);
void LIBOBD2_UART_SendPulse(void* handle, bool pulse);
void LIBOBD2_UART_SwitchMode(void* handle, uint8_t mode);


#endif /* UART_TRANSPORT_PORT_H */
