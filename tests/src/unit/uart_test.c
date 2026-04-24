/* ================================================ INCLUDES =============================================== */
#include "uart_test.h"
#include "kwp_timer.h"
#include "libobd2_test_utils.h"
#include "uart_kwp_transport_port.h"
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

    UART_KWP_WriteByte(handleTester, 0x1);
    KWP_TMR_DelayMs(tmrHandle, 1);
    UART_KWP_RecvByte(handleTester, &byte);
    TEST_ASSERT_EQUAL_HEX8(1, byte);
    UART_KWP_RecvByte(handleEcu, &byte);
    TEST_ASSERT_EQUAL_HEX8(1, byte);

    UART_KWP_WriteByte(handleEcu, 0x42);
    KWP_TMR_DelayMs(tmrHandle, 1);
    UART_KWP_RecvByte(handleTester, &byte);
    TEST_ASSERT_EQUAL_HEX8(0x42, byte);
    UART_KWP_RecvByte(handleEcu, &byte);
    TEST_ASSERT_EQUAL_HEX8(0x42, byte);
}
