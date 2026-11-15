/* ================================================ INCLUDES =============================================== */
#include "fault_injection_test.h"
#include "fault_injection.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define TIME_STEP 10u

/* ============================================ LOCAL VARIABLES ============================================ */
static virtTmrCtx_t timer;
static virtUartLinkCtx_t link;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* Fresh virtual line, no injector */
static void setUpLink(void)
{
    timer = (virtTmrCtx_t){ .timeStep = TIME_STEP };
    link  = (virtUartLinkCtx_t){ .tmrHandle = &timer };
    TEST_ASSERT_TRUE(UART_Init(&link));
}

/* Put 'cnt' bytes on the line, 'first' upwards, one ms apart starting at t=0 */
static void writeBytes(uint8_t first, uint8_t cnt)
{
    for (uint8_t idx = 0u; idx < cnt; idx++)
    {
        timer.timeNow = idx;
        UART_WriteByte(&link, (uint8_t)(first + idx));
    }
    timer.timeNow = cnt;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * Attaching rejects NULL arguments and timer rules whose effect does
 * not fit their operation.
 ******************************************************************/
void test_FAULT_Attach_InvalidArgs(void)
{
    faultUartInjector_t uartInj = {0};
    faultTmrInjector_t tmrInj = { .rule = { .op = VIRT_TMR_OP_START_TIMEOUT, .effect = FAULT_TMR_FAIL } };

    UART_TRACE_Begin("test_FAULT_Attach_InvalidArgs.vcd");
    setUpLink();

    TEST_ASSERT_FALSE(FAULT_UartAttach(NULL, &uartInj));
    TEST_ASSERT_FALSE(FAULT_UartAttach(&link, NULL));
    TEST_ASSERT_FALSE(FAULT_TmrAttach(NULL, &tmrInj));
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, NULL));

    tmrInj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_GET_TIME, .effect = FAULT_TMR_FAIL };
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &tmrInj));
    tmrInj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_DELAY, .effect = FAULT_TMR_EXPIRE };
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &tmrInj));
    tmrInj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_START_TIMEOUT, .effect = FAULT_TMR_SHIFT };
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &tmrInj));
    tmrInj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_INIT };
    TEST_ASSERT_FALSE(FAULT_TmrAttach(&timer, &tmrInj));

    TEST_ASSERT_NULL(link.faults.onWrite);
    TEST_ASSERT_NULL(timer.faults.onOp);
    UART_TRACE_End();
}

/******************************************************************
 * An onWrite rule hits only the bytes in its window: bytes 1 and 2 of
 * 10 11 12 13 arrive with a framing error and bit 0 flipped.
 ******************************************************************/
void test_FAULT_Uart_WriteWindow(void)
{
    const obd_uart_rx_status_t expStatus[] = { OBD_UART_RX_OK, OBD_UART_RX_FRAMING_ERR, OBD_UART_RX_FRAMING_ERR, OBD_UART_RX_OK };
    const uint8_t expByte[] = { 0x10u, 0x10u, 0x13u, 0x13u };
    faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_FRAMING_ERR, .xorMask = 0x01u, .firstByte = 1u, .count = 2u } };
    uint8_t received = 0u;

    UART_TRACE_Begin("test_FAULT_Uart_WriteWindow.vcd");
    setUpLink();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&link, &inj));
    writeBytes(0x10u, 4u);

    for (uint8_t idx = 0u; idx < 4u; idx++)
    {
        TEST_ASSERT_EQUAL(expStatus[idx], UART_RecvByte(&link, &received));
        TEST_ASSERT_EQUAL_HEX8(expByte[idx], received);
    }

    TEST_ASSERT_EQUAL_UINT32(2u, inj.injected);
    TEST_ASSERT_EQUAL_UINT32(4u, inj.written);
    TEST_ASSERT_EQUAL_UINT32(4u, inj.read);
    UART_TRACE_End();
}

/******************************************************************
 * An onRead rule with count 0 hits every byte from firstByte on:
 * reads 1 and 2 report an overrun, read 0 does not.
 ******************************************************************/
void test_FAULT_Uart_ReadFromByte(void)
{
    faultUartInjector_t inj = { .onRead = { .error = OBD_UART_RX_OVERRUN_ERR, .firstByte = 1u } };
    uint8_t received = 0u;

    UART_TRACE_Begin("test_FAULT_Uart_ReadFromByte.vcd");
    setUpLink();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&link, &inj));
    writeBytes(0x20u, 3u);

    TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL(OBD_UART_RX_OVERRUN_ERR, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL(OBD_UART_RX_OVERRUN_ERR, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(0x22u, received);
    TEST_ASSERT_EQUAL_UINT32(2u, inj.injected);
    UART_TRACE_End();
}

/******************************************************************
 * A rule with only an xorMask corrupts the byte but reports no error
 * (noise the UART did not notice); an all-zero rule does nothing.
 ******************************************************************/
void test_FAULT_Uart_XorOnlyAndInactiveRule(void)
{
    faultUartInjector_t inj = { .onWrite = { .xorMask = 0xFFu, .count = 1u } };
    uint8_t received = 0u;

    UART_TRACE_Begin("test_FAULT_Uart_XorOnlyAndInactiveRule.vcd");
    setUpLink();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&link, &inj));
    writeBytes(0x0Fu, 2u);

    TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(0xF0u, received);
    TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&link, &received));
    TEST_ASSERT_EQUAL_HEX8(0x10u, received);
    TEST_ASSERT_EQUAL_UINT32(1u, inj.injected);
    UART_TRACE_End();
}

/******************************************************************
 * After detach the link has no hooks and bytes pass untouched, even
 * across UART_Init.
 ******************************************************************/
void test_FAULT_Uart_Detach(void)
{
    faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_NOISE_ERR } };
    uint8_t received = 0u;

    UART_TRACE_Begin("test_FAULT_Uart_Detach.vcd");
    setUpLink();
    TEST_ASSERT_TRUE(FAULT_UartAttach(&link, &inj));
    FAULT_UartDetach(&link);
    TEST_ASSERT_TRUE(UART_Init(&link));
    writeBytes(0x30u, 1u);

    TEST_ASSERT_EQUAL(OBD_UART_RX_OK, UART_RecvByte(&link, &received));
    TEST_ASSERT_NULL(link.faults.onWrite);
    TEST_ASSERT_NULL(link.faults.onRead);
    TEST_ASSERT_EQUAL_UINT32(0u, inj.injected);
    UART_TRACE_End();
}

/******************************************************************
 * A FAIL rule hits only the calls in its window: the 2nd of three
 * start_timeout calls fails and leaves the timeout unarmed.
 ******************************************************************/
void test_FAULT_Tmr_FailWindow(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_START_TIMEOUT, .effect = FAULT_TMR_FAIL, .firstCall = 1u, .count = 1u } };

    UART_TRACE_Begin("test_FAULT_Tmr_FailWindow.vcd");
    setUpLink();
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));

    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, 10u, NULL, NULL));
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StopTimeout(&timer));
    TEST_ASSERT_EQUAL(OBD_TMR_ERR_HW, TMR_StartTimeout(&timer, 10u, NULL, NULL));
    TEST_ASSERT_FALSE(timer.timeoutStarted);
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, 10u, NULL, NULL));

    TEST_ASSERT_EQUAL_UINT32(1u, inj.injected);
    TEST_ASSERT_EQUAL_UINT32(3u, inj.calls[VIRT_TMR_OP_START_TIMEOUT]);
    TEST_ASSERT_EQUAL_UINT32(1u, inj.calls[VIRT_TMR_OP_STOP_TIMEOUT]);
    UART_TRACE_End();
}

/******************************************************************
 * A FAIL rule on INIT makes TMR_Init fail and leaves the clock as it
 * was; after detach TMR_Init works again.
 ******************************************************************/
void test_FAULT_Tmr_FailInit(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_INIT, .effect = FAULT_TMR_FAIL } };

    UART_TRACE_Begin("test_FAULT_Tmr_FailInit.vcd");
    setUpLink();
    timer.timeNow = 500u;
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));

    TEST_ASSERT_EQUAL(OBD_TMR_ERR_HW, TMR_Init(&timer));
    TEST_ASSERT_EQUAL_UINT32(500u, timer.timeNow);

    FAULT_TmrDetach(&timer);
    TEST_ASSERT_NULL(timer.faults.onOp);
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_Init(&timer));
    TEST_ASSERT_EQUAL_UINT32(0u, timer.timeNow);
    UART_TRACE_End();
}

/******************************************************************
 * EXPIRE forces is_timeout_expired; SHIFT moves get_time (clamped at
 * 0) and stretches delay.
 *   1. EXPIRE: a timeout armed for 1000 ms reports expired at once
 *   2. SHIFT get_time by -50 at t=30: the first read gives 0, the next 30
 *   3. SHIFT delay by +100: a 10 ms delay moves the clock 110 ms
 ******************************************************************/
void test_FAULT_Tmr_ExpireAndShift(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_IS_TIMEOUT_EXPIRED, .effect = FAULT_TMR_EXPIRE } };

    UART_TRACE_Begin("test_FAULT_Tmr_ExpireAndShift.vcd");
    setUpLink();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));
    TEST_ASSERT_EQUAL(OBD_TMR_OK, TMR_StartTimeout(&timer, 1000u, NULL, NULL));
    TEST_ASSERT_TRUE(TMR_IsTimeoutExpired(&timer));

    /* 2. */
    timer.timeNow = 30u;
    inj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_GET_TIME, .effect = FAULT_TMR_SHIFT, .shiftMs = -50, .count = 1u };
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));
    TEST_ASSERT_EQUAL_UINT32(0u, TMR_GetTimeMs(&timer));
    TEST_ASSERT_EQUAL_UINT32(30u, TMR_GetTimeMs(&timer));

    /* 3. */
    inj.rule = (faultTmrRule_t){ .op = VIRT_TMR_OP_DELAY, .effect = FAULT_TMR_SHIFT, .shiftMs = 100 };
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&timer, &inj));
    TMR_DelayMs(&timer, 10u);
    TEST_ASSERT_EQUAL_UINT32(140u, timer.timeNow);
    TEST_ASSERT_EQUAL_UINT32(1u, inj.injected);
    UART_TRACE_End();
}
