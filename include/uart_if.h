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

typedef struct
{
    bool (*init)(void *handle);
    bool (*recv_byte)(void *handle, uint8_t *buffer);
    void (*flush_rx)(void *handle);
    void (*send_byte)(void *handle, uint8_t data);
    void (*send_pulse)(void *handle, bool pulse);
    void (*switch_mode)(void *handle, uint8_t mode);
} obd_uart_ops_t;

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */

#endif /* TRANSPORT_IF_H */
