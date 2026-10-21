/* ================================================ INCLUDES =============================================== */
#include "uart_test.h"
#include "libobd2_timer_port.h"
#include "libobd2_test_utils.h"
#include "libobd2_uart_port.h"
#include "unity.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_UART_000(void)
{
    void *handleTester = (void*)&uartCtxTx;
    void *handleEcu = (void*)&uartCtxRx;
    void *tmrHandle = (void*)&tmrCtxTx;
    uint8_t byte = 0;

    LIBOBD2_UART_WriteByte(handleTester, 0x1);
    LIBOBD2_TMR_DelayMs(tmrHandle, 1);
    LIBOBD2_UART_RecvByte(handleTester, &byte);
    TEST_ASSERT_EQUAL_HEX8(1, byte);
    LIBOBD2_UART_RecvByte(handleEcu, &byte);
    TEST_ASSERT_EQUAL_HEX8(1, byte);

    LIBOBD2_UART_WriteByte(handleEcu, 0x42);
    LIBOBD2_TMR_DelayMs(tmrHandle, 1);
    LIBOBD2_UART_RecvByte(handleTester, &byte);
    TEST_ASSERT_EQUAL_HEX8(0x42, byte);
    LIBOBD2_UART_RecvByte(handleEcu, &byte);
    TEST_ASSERT_EQUAL_HEX8(0x42, byte);
}
