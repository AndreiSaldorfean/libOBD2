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
    obd_status_t (*send_byte)(void *handle, uint8_t data);
    obd_status_t (*recv_byte)(void *handle, uint8_t *buffer);
    obd_status_t (*send_pulse)(void *handle, bool pulse);
} obd_transport_ops_t;

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */

#endif /* TRANSPORT_IF_H */
