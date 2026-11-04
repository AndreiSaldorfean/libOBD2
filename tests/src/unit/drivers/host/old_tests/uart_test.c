/* ================================================ INCLUDES =============================================== */
#include "uart_test.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include "theft.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern bool UART_Init(void* handle);
extern bool UART_RecvByte(void* handle, uint8_t *recv_buffer);
extern void UART_WriteByte(void* handle, uint8_t data);
extern void UART_FlushRx(void* handle);
extern void UART_SendPulse(void* handle, bool pulse);
extern void UART_SwitchMode(void* handle, uint8_t mode);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_UART_WriteByte_0(void)
{
    TEST_ASSERT_EQUAL_UINT32(1u,0);
}
