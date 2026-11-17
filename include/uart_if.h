#ifndef TRANSPORT_IF_H
#define TRANSPORT_IF_H

/*************************************************************************************************************
 * This header file provides the interface functions required for higher level api's. The integrator
 * must provide the implementations specified in the structs.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>
#include <stdbool.h>

/* ================================================= MACROS ================================================ */
/* Line modes passed to switch_mode */
#define FAST_INIT_WAKEUP_START  (0x1) /* TX pin driven as GPIO for the fast-init wake-up pattern */
#define FAST_INIT_WAKEUP_END    (0x2) /* TX pin handed back to the UART */
#define DIGITAL_MODE_BEGIN      (0x3) /* TX pin driven as GPIO for 5-baud bit-banging */
#define DIGITAL_MODE_END        (0x4) /* TX pin handed back to the UART */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* Result of recv_byte. A byte is in the buffer for OK and for every error. */
typedef enum
{
    OBD_UART_RX_OK = 0U,     /* a byte was received without error */
    OBD_UART_RX_NO_DATA,     /* nothing received yet, buffer untouched */
    OBD_UART_RX_PARITY_ERR,
    OBD_UART_RX_FRAMING_ERR, /* stop bit not found */
    OBD_UART_RX_OVERRUN_ERR, /* a byte arrived before the previous one was read; one was lost */
    OBD_UART_RX_NOISE_ERR,
} obd_uart_rx_status_t;

typedef struct
{
    bool (*init)(void *handle);
    obd_uart_rx_status_t (*recv_byte)(void *handle, uint8_t *buffer);
    void (*flush_rx)(void *handle); /* optional: drops pending RX bytes (e.g. the K-line echo) */
    void (*send_byte)(void *handle, uint8_t data);
    void (*send_pulse)(void *handle, bool pulse);
    void (*switch_mode)(void *handle, uint8_t mode);
} obd_uart_ops_t;

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */

#endif /* TRANSPORT_IF_H */
