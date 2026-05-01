#ifndef UART_TRANSPORT_PORT_H
#define UART_TRANSPORT_PORT_H

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>
#include <stdbool.h>
#include "statusRetCodes.h"

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
    uint32_t gpioOutType;
    uint32_t gpioOutSpeed;
    uint32_t gpio;

    bool init;

}uart_ctx_t;
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
bool UART_Init(void* handle);
bool UART_RecvByte(void* handle, uint8_t *recv_buffer);
void UART_WriteByte(void* handle, uint8_t data);
void UART_FlushRx(void* handle);
void UART_SendPulse(void* handle, bool pulse);
void UART_SwitchMode(void* handle, uint8_t mode);


#endif /* UART_TRANSPORT_PORT_H */
