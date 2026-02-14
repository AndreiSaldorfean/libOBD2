#ifndef UART_KWP_TRANSPORT_PORT_H
#define UART_KWP_TRANSPORT_PORT_H

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>
#include <stdbool.h>
#include "srv_status.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint32_t usartClk;
    uint32_t baudRate;
    uint32_t dataBits;
    uint32_t stopBits;
    uint32_t mode;
    bool parity;
    uint32_t flowControl;
    uint32_t usartNum;
    uint32_t usartTxPin;
    uint32_t usartRxPin;

    uint32_t gpioRcc;
    uint32_t gpio;

    bool init;

}uartKwp_ctx_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t UART_KWP_Init(void* handle);
obd_status_t UART_KWP_WriteByte(void* handle, uint8_t data);
obd_status_t UART_KWP_RecvByte(void* handle, uint8_t *recv_buffer);
obd_status_t UART_KWP_SendPulse(void* handle, bool pulse);
void UART_KWP_ChangeBaud(void* handle, uint8_t mode);


#endif /* UART_KWP_TRANSPORT_PORT_H */
