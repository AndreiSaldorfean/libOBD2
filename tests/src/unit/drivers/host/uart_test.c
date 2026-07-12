/* ================================================ INCLUDES =============================================== */
#include "uart_test.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern bool LIBOBD2_UART_Init(void* handle);
extern bool LIBOBD2_UART_RecvByte(void* handle, uint8_t *recv_buffer);
extern void LIBOBD2_UART_WriteByte(void* handle, uint8_t data);
extern void LIBOBD2_UART_FlushRx(void* handle);
extern void LIBOBD2_UART_SendPulse(void* handle, bool pulse);
extern void LIBOBD2_UART_SwitchMode(void* handle, uint8_t mode);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_UART_WriteByte_0(void)
{
    TEST_ASSERT_EQUAL_UINT32(1u,0);
}
