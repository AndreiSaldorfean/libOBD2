#ifndef UART_TRANSPORT_PORT_H
#define UART_TRANSPORT_PORT_H

/* ================================================ INCLUDES =============================================== */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "cQueue.h"
#include "libobd2_timer_port.h"
#include "statusRetCodes.h"
#include "uart_if.h"

/* ================================================= MACROS ================================================ */
#define VIRT_UART_QUEUE_SIZE         (128U)   /* bytes the virtual line can hold */
#define VIRT_UART_BITBANG_FRAME_BITS (10U)    /* start + 8 data + stop */
#define VIRT_UART_BITBANG_DATA_SHIFT (1U)     /* data bits follow the start bit */
#define VIRT_UART_BITBANG_DATA_MASK  (0xFFU)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* One byte on the virtual line */
typedef struct
{
    uint8_t byte;
    uint32_t timeReceived;
    obd_uart_rx_status_t error; /* OBD_UART_RX_OK, or the error the receiver reports for this byte */
}uartEvent_t;

/* Error-injection seam. A NULL hook injects nothing. */
typedef struct
{
    /* Called for every byte put on the line; may change event->byte or set event->error */
    void (*onWrite)(void *pUserData, uartEvent_t *event);
    /* Called for every byte handed to the receiver; may set event->error (e.g. overrun) */
    void (*onRead)(void *pUserData, uartEvent_t *event);
    void *pUserData;
}virtUartFaultHooks_t;

typedef struct
{
    Queue_t eventQueue;                           /* the virtual line, set up by UART_Init */
    uartEvent_t eventStore[VIRT_UART_QUEUE_SIZE]; /* storage of eventQueue, no heap needed */
    virtTmrCtx_t *tmrHandle;
    virtUartFaultHooks_t faults;

    /* Line mode and 5-baud bit-bang state, reset by UART_Init */
    uint8_t mode;
    uint16_t bitbangedByte;
    uint16_t bitbangIdx;
}virtUartLinkCtx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
bool UART_Init(void* handle);
obd_uart_rx_status_t UART_RecvByte(void* handle, uint8_t *recv_buffer);
void UART_WriteByte(void* handle, uint8_t data);
void UART_FlushRx(void* handle);
void UART_SendPulse(void* handle, bool pulse);
void UART_SwitchMode(void* handle, uint8_t mode);

#endif /* UART_TRANSPORT_PORT_H */
