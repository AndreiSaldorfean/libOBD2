#ifndef LIBOBD2_TEST_H
#define LIBOBD2_TEST_H
/* ================================================ INCLUDES =============================================== */
#include "l2_kwp.h"
#include "kwp_timer.h"
#include "uart_kwp_transport_port.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void test_LibOBD2_Init_000(void);
void test_LibOBD2_RequestService_000(void);

#endif /* LIBOBD2_TEST_H */
