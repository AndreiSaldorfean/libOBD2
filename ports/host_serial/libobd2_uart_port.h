#ifndef UART_TRANSPORT_PORT_H
#define UART_TRANSPORT_PORT_H

/*************************************************************************************************************
 * K-line port for x86 hosts (Linux) talking through a serial adapter, typically a USB "KKL" / FTDI cable,
 * on top of libserialport. The 5-baud and fast-init wake-up patterns are produced with the serial BREAK
 * condition (TX held low), so no GPIO access is needed.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "libserialport.h"
#include "statusRetCodes.h"
#include "uart_if.h"

/* ================================================= MACROS ================================================ */
#define SERIAL_UART_DEFAULT_BAUD      (10400)  /* ISO9141-2 / KWP2000 K-line baud rate */
#define SERIAL_UART_DEFAULT_SETTLE_MS (5U)     /* time FlushRx gives the echo of the last byte to come back */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    /* Set by the integrator */
    const char *portName;   /* OS name of the port, e.g. "/dev/ttyUSB0" */
    int baudRate;           /* 0 selects SERIAL_UART_DEFAULT_BAUD */
    uint32_t flushSettleMs; /* 0 selects SERIAL_UART_DEFAULT_SETTLE_MS */

    /* Owned by the port */
    struct sp_port *port;   /* opened by UART_Init, closed by UART_Deinit */
    bool breakActive;       /* TX held low (BREAK) by UART_SendPulse */
}serialUartCtx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
bool UART_Init(void* handle);
obd_uart_rx_status_t UART_RecvByte(void* handle, uint8_t *recv_buffer);
void UART_WriteByte(void* handle, uint8_t data);
void UART_FlushRx(void* handle);
void UART_SendPulse(void* handle, bool pulse);
void UART_SwitchMode(void* handle, uint8_t mode);

/* Host only: releases the port opened by UART_Init */
void UART_Deinit(void* handle);

#endif /* UART_TRANSPORT_PORT_H */
