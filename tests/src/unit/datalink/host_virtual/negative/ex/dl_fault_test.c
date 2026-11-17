/* ================================================ INCLUDES =============================================== */
#include "dl_fault_test.h"
#include "dl_fixture.h"
#include "datalink.h"
#include "fault_injection.h"
#include "iso9141.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "statusRetCodes.h"
#include "uart_trace.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ECU answer to "01 05" (coolant temperature): 48 6B 12 41 05 7B 86 */
static const uint8_t coolantResp[] = { 0x48u, 0x6Bu, 0x12u, 0x41u, 0x05u, 0x7Bu, 0x86u };

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A framing error on a response data byte
 * - description: The broken byte is reported as OBD_ERR_UART_FRAMING,
 *                not as a broken frame or a timeout.
 *                ECU: 48 6B 12 41 [05 framing] 7B 86
 * - steps:
 *     1. Inject a framing error on line byte 4
 *     2. Queue the coolant answer
 *     3. ISO9141_RecvResponse returns OBD_ERR_UART_FRAMING, timeout OK
 *     4. Exactly one fault was injected
 ******************************************************************/
void test_neg_ex_DLFAULT_RecvResponse_0(void)
{
    faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_FRAMING_ERR, .firstByte = 4u, .count = 1u } };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_RecvResponse_0.vcd");
    DLFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_UartAttach(&dlFx.uart, &inj));

    /* 2. */
    DLFX_QueueEcuFrame(coolantResp, sizeof(coolantResp));

    /* 3. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_FRAMING, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 4. */
    TEST_ASSERT_EQUAL_UINT32(1u, inj.injected);
}

/******************************************************************
 * - brief:       An overrun on the format byte
 * - description: The very first byte of the response is lost to an
 *                overrun; the error is reported as OBD_ERR_UART_OVERRUN.
 * - steps:
 *     1. Inject an overrun on the first read
 *     2. Queue the coolant answer
 *     3. ISO9141_RecvResponse returns OBD_ERR_UART_OVERRUN, timeout OK
 ******************************************************************/
void test_neg_ex_DLFAULT_RecvResponse_1(void)
{
    faultUartInjector_t inj = { .onRead = { .error = OBD_UART_RX_OVERRUN_ERR, .count = 1u } };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_RecvResponse_1.vcd");
    DLFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_UartAttach(&dlFx.uart, &inj));

    /* 2. */
    DLFX_QueueEcuFrame(coolantResp, sizeof(coolantResp));

    /* 3. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_OVERRUN, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
}

/******************************************************************
 * - brief:       A silently corrupted byte breaks the checksum
 * - description: A data byte flipped without a UART error (05 -> 04)
 *                means no byte matches the running checksum, so the
 *                read times out waiting for one more.
 * - steps:
 *     1. Flip bit 0 of line byte 4, no error flag
 *     2. Queue the coolant answer
 *     3. RecvResponse returns OBD_ERR_COMM_DATA_BYTE_NOT_RECVD with
 *        OBD_ERR_TIMEOUT_MAX
 ******************************************************************/
void test_neg_ex_DLFAULT_RecvResponse_2(void)
{
    faultUartInjector_t inj = { .onWrite = { .xorMask = 0x01u, .firstByte = 4u, .count = 1u } };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_RecvResponse_2.vcd");
    DLFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_UartAttach(&dlFx.uart, &inj));

    /* 2. */
    DLFX_QueueEcuFrame(coolantResp, sizeof(coolantResp));

    /* 3. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_DATA_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TIMEOUT_MAX, status.timeout);
}

/******************************************************************
 * - brief:       A parity error on keyword byte 1 fails the init
 * - description: The 5-baud init stops with OBD_ERR_UART_PARITY and
 *                neither a protocol nor a header is stored.
 * - steps:
 *     1. Inject a parity error on the 2nd ECU byte (KB1)
 *     2. Queue a correct ECU 5-baud answer
 *     3. ISO9141_Connect returns OBD_ERR_UART_PARITY
 *     4. Protocol untouched, no header stored
 ******************************************************************/
void test_neg_ex_DLFAULT_Connect_0(void)
{
    faultUartInjector_t inj = { .onWrite = { .error = OBD_UART_RX_PARITY_ERR, .firstByte = 1u, .count = 1u } };
    const dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;
    uint8_t protocol = 0xFFu;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_Connect_0.vcd");
    DLFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_UartAttach(&dlFx.uart, &inj));

    /* 2. */
    DLFX_QueueEcu5BaudInit(&ecu);

    /* 3. */
    status = ISO9141_Connect(&dlFx.link, &protocol);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_PARITY, status.response);

    /* 4. */
    TEST_ASSERT_EQUAL_HEX8(0xFFu, protocol);
    TEST_ASSERT_EQUAL_HEX8(0x00u, dlFx.proto.header.fmt);
}

/******************************************************************
 * - brief:       Noise on the sync byte fails the init
 * - description: The first byte the tester reads during the 5-baud
 *                init arrives with noise: OBD_ERR_UART_NOISE.
 * - steps:
 *     1. Inject noise on the first read
 *     2. Queue a correct ECU 5-baud answer
 *     3. ISO9141_Connect returns OBD_ERR_UART_NOISE, protocol untouched
 ******************************************************************/
void test_neg_ex_DLFAULT_Connect_1(void)
{
    faultUartInjector_t inj = { .onRead = { .error = OBD_UART_RX_NOISE_ERR, .count = 1u } };
    const dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;
    uint8_t protocol = 0xFFu;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_Connect_1.vcd");
    DLFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_UartAttach(&dlFx.uart, &inj));

    /* 2. */
    DLFX_QueueEcu5BaudInit(&ecu);

    /* 3. */
    status = ISO9141_Connect(&dlFx.link, &protocol);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_NOISE, status.response);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, protocol);
}

/******************************************************************
 * - brief:       A timer that fails to init stops DL_Connect
 * - description: DL_Connect reports OBD_ERR_CONN_TIMER_INIT_FAILED
 *                before any protocol is tried.
 * - steps:
 *     1. Inject a FAIL on timer init
 *     2. DL_Connect returns OBD_ERR_CONN_TIMER_INIT_FAILED
 *     3. Nothing bound, no delay ever ran (no 5-baud wake-up)
 ******************************************************************/
void test_neg_ex_DLFAULT_Connect_2(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_INIT, .effect = FAULT_TMR_FAIL } };
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_Connect_2.vcd");
    DLFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&dlFx.timer, &inj));

    /* 2. */
    status = DL_Connect(&dlFx.link);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_CONN_TIMER_INIT_FAILED, status.response);

    /* 3. */
    TEST_ASSERT_NULL(dlFx.link.send_request);
    TEST_ASSERT_EQUAL_UINT32(0u, inj.calls[VIRT_TMR_OP_DELAY]);
}

/******************************************************************
 * - brief:       The P2 timeout cannot be started after a request
 * - description: The request goes out, but the P2 window for the
 *                answer cannot be armed: OBD_ERR_TMR_START_FAILED.
 * - steps:
 *     1. Inject a FAIL on the first start_timeout
 *     2. ISO9141_SendRequest returns OBD_ERR_TMR_START_FAILED
 *     3. Exactly one fault was injected
 ******************************************************************/
void test_neg_ex_DLFAULT_SendRequest_0(void)
{
    const obd_data_t request = { .sid = 0x01u, .param = { 0x05u } };
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_START_TIMEOUT, .effect = FAULT_TMR_FAIL, .count = 1u } };
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_SendRequest_0.vcd");
    DLFX_SetUpLine();

    /* 1. */
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&dlFx.timer, &inj));

    /* 2. */
    status = ISO9141_SendRequest(&dlFx.link, &request, 2u);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TMR_START_FAILED, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(1u, inj.injected);
}

/******************************************************************
 * - brief:       The P2 timeout cannot be stopped
 * - description: The format byte arrives, but stopping the P2 window
 *                fails: ISO9141_RecvResponse reports
 *                OBD_ERR_TMR_STOP_FAILED.
 * - steps:
 *     1. Queue the coolant answer
 *     2. Inject a FAIL on stop_timeout
 *     3. RecvResponse returns OBD_ERR_TMR_STOP_FAILED, timeout OK
 ******************************************************************/
void test_neg_ex_DLFAULT_RecvResponse_3(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_STOP_TIMEOUT, .effect = FAULT_TMR_FAIL } };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_RecvResponse_3.vcd");
    DLFX_SetUpLine();

    /* 1. */
    DLFX_QueueEcuFrame(coolantResp, sizeof(coolantResp));

    /* 2. */
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&dlFx.timer, &inj));

    /* 3. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_TMR_STOP_FAILED, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
}

/******************************************************************
 * - brief:       P2 expires before the format byte arrives
 * - description: The timer reports the P2 window as expired right
 *                away: the response is missing with the P2 max
 *                timeout.
 * - steps:
 *     1. Queue the coolant answer
 *     2. Force every is_timeout_expired to true
 *     3. RecvResponse returns OBD_ERR_COMM_FMT_BYTE_NOT_RECVD with
 *        OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU
 ******************************************************************/
void test_neg_ex_DLFAULT_RecvResponse_4(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_IS_TIMEOUT_EXPIRED, .effect = FAULT_TMR_EXPIRE } };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_RecvResponse_4.vcd");
    DLFX_SetUpLine();

    /* 1. */
    DLFX_QueueEcuFrame(coolantResp, sizeof(coolantResp));

    /* 2. */
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&dlFx.timer, &inj));

    /* 3. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU, status.timeout);
}

/******************************************************************
 * - brief:       The clock jumps back during the P2 min check
 * - description: get_time reads 40 ms too early once, so the ECU looks
 *                like it answered before P2 min: the response is
 *                rejected with the P2 min timeout.
 * - steps:
 *     1. Queue the coolant answer
 *     2. Shift the next get_time by -40 ms
 *     3. RecvResponse returns OBD_ERR_COMM_FMT_BYTE_NOT_RECVD with
 *        OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU
 ******************************************************************/
void test_neg_ex_DLFAULT_RecvResponse_5(void)
{
    faultTmrInjector_t inj = { .rule = { .op = VIRT_TMR_OP_GET_TIME, .effect = FAULT_TMR_SHIFT, .shiftMs = -40, .count = 1u } };
    obd_data_t resp = {0};
    size_t len = 0u;
    obd_status_t status = {0};

    UART_TRACE_Begin("test_neg_ex_DLFAULT_RecvResponse_5.vcd");
    DLFX_SetUpLine();

    /* 1. */
    DLFX_QueueEcuFrame(coolantResp, sizeof(coolantResp));

    /* 2. */
    TEST_ASSERT_TRUE(FAULT_TmrAttach(&dlFx.timer, &inj));

    /* 3. */
    status = ISO9141_RecvResponse(&dlFx.link, &resp, &len);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_FMT_BYTE_NOT_RECVD, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU, status.timeout);
}
