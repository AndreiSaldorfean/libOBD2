#ifndef TRANSPORT_IF_H
#define TRANSPORT_IF_H

/*************************************************************************************************************
 * This header file provides the interface functions required for higher level api's. The integrator
 * must provide the implementations specified in the structs.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"
#include <stddef.h>
#include <stdbool.h>

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */

typedef struct
{
    obd_status_t (*init)(void *handle);
    void (*send_byte)(void *handle, uint8_t data);
    void (*recv_byte)(void *handle, uint8_t *buffer);
    void (*send_pulse)(void *handle, bool pulse);
    void (*switch_mode)(void *handle, uint8_t mode);
} obd_transport_ops_t;

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */

#endif /* TRANSPORT_IF_H */
