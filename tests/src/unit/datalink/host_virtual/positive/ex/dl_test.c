/* ================================================ INCLUDES =============================================== */
#include "dl_test.h"
#include "dl_fixture.h"
#include "datalink.h"
#include "datalink_utils.h"
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
/* flush_rx op that only counts its calls */
static size_t flushCnt;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void countFlush(void *handle)
{
    (void)handle;
    flushCnt++;
}

/* ================================================ MODULE API ============================================= */
/******************************************************************
 * - brief:       A byte inside [timeMin, timeMax] is returned OK
 * - description: The ECU byte arrives inside the P2 window; the read
 *                returns it with no error and reports how long it
 *                took.
 * - steps:
 *     1. Byte is on the line at t=30, window is [P2 min, P2 max]
 *     2. Tester polls at 0, 10, 20, 30 (not yet there) and 40 (read)
 *     3. Elapsed time is 50 ms: 5 polls of 10 ms
 ******************************************************************/
void test_pos_ex_DL_ReadByteInTimeframe_0(void)
{
    uint8_t byte = 0u;
    uint32_t elapsed = 0u;
    obd_status_t status = {0};

    /* 1. */
    DLFX_SetUpConnectedLine();
    DLFX_LineWriteAt(30u, 0x55u);

    /* 2. */
    status = DL_ReadByteInTimeframe(&dlFx.link, &elapsed, &byte, P2_TIME_MIN, P2_TIME_MAX);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);
    TEST_ASSERT_EQUAL_HEX8(0x55u, byte);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(5u * DLFX_TIME_STEP, elapsed);
}

/******************************************************************
 * - brief:       RecvByteBlocking waits as long as needed
 * - description: A byte that only shows up at t=1000 (far beyond any
 *                protocol window) is still returned.
 * - steps:
 *     1. Put 0xA5 on the line at t=1000
 *     2. DL_RecvByteBlocking returns true with 0xA5
 *     3. The clock is past 1000
 ******************************************************************/
void test_pos_ex_DL_RecvByteBlocking_0(void)
{
    uint8_t byte = 0u;

    /* 1. */
    DLFX_SetUpConnectedLine();
    DLFX_LineWriteAt(1000u, 0xA5u);

    /* 2. */
    TEST_ASSERT_TRUE(DL_RecvByteBlocking(&dlFx.link, &byte));
    TEST_ASSERT_EQUAL_HEX8(0xA5u, byte);

    /* 3. */
    TEST_ASSERT_GREATER_THAN_UINT32(1000u, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       Send 0x33 at 5 baud, the ISO9141 wake-up address
 * - description: DL_SendByteBitBanged drives the line by hand: digital
 *                mode around the frame, LSB first, 200 ms per bit.
 * - steps:
 *     1. Line switches to digital mode, then back at the end
 *     2. Start bit low, 8 data bits LSB first, stop bit high
 *     3. Every bit lasts 200 ms, 2 s for the whole byte
 ******************************************************************/
void test_pos_ex_DL_SendByteBitBanged_0(void)
{
    /* 0x33 = 0b00110011, LSB first */
    const uint8_t expLevel[DLFX_BITBANG_BITS] = { PULSE_LOW,
                                                  1, 1, 0, 0, 1, 1, 0, 0,
                                                  PULSE_HIGH };

    DLFX_SetUpConnectedLine();

    UART_TRACE_Begin("test_pos_ex_DL_SendByteBitBanged_0.vcd");
    DL_SendByteBitBanged(&dlFx.link, DLFX_TARGET_ADDR, 5u);
    UART_TRACE_End();

    /* 1. */
    TEST_ASSERT_EQUAL_size_t(2u, dlFx.modeCnt);
    TEST_ASSERT_EQUAL_UINT8(DIGITAL_MODE_BEGIN, dlFx.modeSeq[0]);
    TEST_ASSERT_EQUAL_UINT8(DIGITAL_MODE_END, dlFx.modeSeq[1]);

    /* 2. */
    TEST_ASSERT_EQUAL_size_t(DLFX_BITBANG_BITS, dlFx.pulseCnt);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expLevel, dlFx.pulseLevel, DLFX_BITBANG_BITS);

    /* 3. */
    for (size_t bit = 0; bit < DLFX_BITBANG_BITS; bit++)
    {
        TEST_ASSERT_EQUAL_UINT32(bit * DLFX_BITBANG_BIT_TIME, dlFx.pulseTime[bit]);
    }
    TEST_ASSERT_EQUAL_UINT32(DLFX_BITBANG_BITS * DLFX_BITBANG_BIT_TIME, dlFx.timer.timeNow);
}

/******************************************************************
 * - brief:       Connect to an ISO9141-2 ECU through DL_Connect
 * - description: The common datalink tries the supported protocols;
 *                ISO9141 completes its 5-baud init and DL_Connect
 *                binds the link to it.
 * - steps:
 *     1. ECU answers the 5 baud init with 55 08 08 and CC
 *     2. DL_Connect reports OK
 *     3. send/recv are bound to ISO9141 with the 68 6A F1 header
 ******************************************************************/
void test_pos_ex_DL_Connect_0(void)
{
    const dlFxEcuInit_t ecu = DLFX_ECU_INIT_OK;
    obd_status_t status = {0};

    DLFX_SetUpConnectedLine();
    dlFx.link.connect       = NULL;
    dlFx.link.send_request  = NULL;
    dlFx.link.recv_response = NULL;

    /* 1. */
    DLFX_QueueEcu5BaudInit(&ecu);

    /* 2. */
    UART_TRACE_Begin("test_pos_ex_DL_Connect_0.vcd");
    status = DL_Connect(&dlFx.link);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 3. */
    TEST_ASSERT_EQUAL_PTR(ISO9141_Connect, dlFx.link.connect);
    TEST_ASSERT_EQUAL_PTR(ISO9141_SendRequest, dlFx.link.send_request);
    TEST_ASSERT_EQUAL_PTR(ISO9141_RecvResponse, dlFx.link.recv_response);
    TEST_ASSERT_EQUAL_HEX8(0x68u, dlFx.proto.header.fmt);
    TEST_ASSERT_EQUAL_HEX8(0x6Au, dlFx.proto.header.trgAddr);
    TEST_ASSERT_EQUAL_HEX8(0xF1u, dlFx.proto.header.srcAddr);
}

/******************************************************************
 * - brief:       Send a request and read it back through the DL API
 * - description: The host UART is a loopback, so the request the
 *                tester sends is what it reads back as the response.
 * - steps:
 *     1. DL_SendRequest puts 68 6A F1 01 05 C9 on the line
 *     2. After P2 min the frame is read back with DL_RecvResponse
 *     3. Only the data part (sid + params) is handed back
 ******************************************************************/
void test_pos_ex_DL_SendRecv_0(void)
{
    const obd_data_t request = { .sid = 0x01u, .param = { 0x05u } };
    obd_data_t resp = {0};
    size_t respLen = 0u;
    obd_status_t status = {0};

    DLFX_SetUpConnectedLine();

    /* 1. */
    UART_TRACE_Begin("test_pos_ex_DL_SendRecv_0.vcd");
    status = DL_SendRequest(&dlFx.link, &request, 2u);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 2. */
    LIBOBD2_TMR_Delay((&dlFx.link), P2_TIME_MIN);
    status = DL_RecvResponse(&dlFx.link, &resp, &respLen);
    UART_TRACE_End();
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.timeout);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(2u, respLen);
    TEST_ASSERT_EQUAL_HEX8(request.sid, resp.sid);
    TEST_ASSERT_EQUAL_HEX8(request.param[0], resp.param[0]);
}

/******************************************************************
 * - brief:       Sending a request ends with the driver's flush_rx
 * - description: On real hardware flush_rx drops the K-line echo of
 *                the request; it must be called once per request.
 * - steps:
 *     1. Replace flush_rx with a counter
 *     2. DL_SendRequest returns OK
 *     3. flush_rx ran exactly once
 ******************************************************************/
void test_pos_ex_DL_SendRequest_0(void)
{
    const obd_data_t request = { .sid = 0x01u, .param = { 0x05u } };
    obd_status_t status = {0};

    DLFX_SetUpConnectedLine();

    /* 1. */
    flushCnt = 0u;
    dlFxUartOps.flush_rx = countFlush;

    /* 2. */
    status = DL_SendRequest(&dlFx.link, &request, 2u);
    dlFxUartOps.flush_rx = UART_FlushRx;
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);

    /* 3. */
    TEST_ASSERT_EQUAL_size_t(1u, flushCnt);
}

/******************************************************************
 * - brief:       Two links sharing timing ops keep their own P2 start
 * - description: The P2 window start (timeSample) lives in the link,
 *                not in the shared ops table.
 * - steps:
 *     1. Copy the link, start a timeout on the first at t=0
 *     2. 100 ms later start a timeout on the copy
 *     3. First link sampled 0, the copy sampled 100
 ******************************************************************/
void test_pos_ex_DL_TimeSample_0(void)
{
    dataLink_if_t other = {0};

    DLFX_SetUpConnectedLine();
    other = dlFx.link;

    /* 1. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, DL_StartTimeout(&dlFx.link, P2_TIME_MAX).response);

    /* 2. */
    TMR_DelayMs(&dlFx.timer, 100u);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, DL_StartTimeout(&other, P2_TIME_MAX).response);

    /* 3. */
    TEST_ASSERT_EQUAL_UINT32(0u, dlFx.link.timeSample);
    TEST_ASSERT_EQUAL_UINT32(100u, other.timeSample);
}

/******************************************************************
 * - brief:       Every recv_byte result maps to its status code
 * - description: The UART receive status is translated into the
 *                obd_status_t response code that reports it; an
 *                unknown value is a generic error.
 * - steps:
 *     1. OK, NO_DATA, PARITY, FRAMING, OVERRUN, NOISE map 1:1
 *     2. 0xFF maps to OBD_GENERIC_ERROR
 ******************************************************************/
void test_pos_ex_DL_UartRxStatusToResponse_0(void)
{
    UART_TRACE_Begin("test_pos_ex_DL_UartRxStatusToResponse_0.vcd");

    /* 1. */
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK,        DL_UartRxStatusToResponse(OBD_UART_RX_OK));
    TEST_ASSERT_EQUAL_HEX16(OBD_RECV_NOT_READY,   DL_UartRxStatusToResponse(OBD_UART_RX_NO_DATA));
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_PARITY,  DL_UartRxStatusToResponse(OBD_UART_RX_PARITY_ERR));
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_FRAMING, DL_UartRxStatusToResponse(OBD_UART_RX_FRAMING_ERR));
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_OVERRUN, DL_UartRxStatusToResponse(OBD_UART_RX_OVERRUN_ERR));
    TEST_ASSERT_EQUAL_HEX16(OBD_ERR_UART_NOISE,   DL_UartRxStatusToResponse(OBD_UART_RX_NOISE_ERR));

    /* 2. */
    TEST_ASSERT_EQUAL_HEX16(OBD_GENERIC_ERROR,    DL_UartRxStatusToResponse((obd_uart_rx_status_t)0xFFu));
    UART_TRACE_End();
}
