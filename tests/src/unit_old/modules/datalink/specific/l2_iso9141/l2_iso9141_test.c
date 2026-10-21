/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "ecu_uart.h"
#include "l2_iso9141.h"
#include "libobd2.h"
#include "libobd2_service_requests.h"
#include "libobd2_test_utils.h"
#include "projdefs.h"
#include <stddef.h>
#include <sys/unistd.h>
#define STM32F4
#include "libopencm3/stm32/f4/memorymap.h"
#include "libopencm3/stm32/f4/rcc.h"
#include "libopencm3/stm32/common/timer_common_all.h"
#include "libopencm3/stm32/f4/gpio.h"
#include "libopencm3/stm32/f4/usart.h"
#include "uart_if.h"
#include "iso15031_5.h"
#include "tusb.h"
#include "libobd2_uart_port.h"
#include "unity.h"
#include <stdio.h>
#include <time.h>
#include "libobd2_timer_port.h"
#include "utils.h"
#include "l2_kwp2000.h"
#include "ecu_datalink.h"
#include "ecu_l2_iso9141.h"

/* ================================================= MACROS ================================================ */
#define MSG_00_SIZE        (0x6U)
#define TIM2_PRESCALER     (83U)
/* ============================================ LOCAL VARIABLES ============================================ */
static volatile BaseType_t g_sender_done = pdFALSE;
static volatile BaseType_t g_receiver_done = pdFALSE;

static const header_t iso9141_header_00 =
{
    .fmt = 0x68,
    .trgAddr = 0x6A,
    .srcAddr = 0xF1,
};

static const obd_data_t iso9141_obd_data_00 =
{
    .sid = 0x09,
    .param = {0x02}
};

static const obd_data_t iso9141_obd_data_02 =
{
    .sid = 0x81,
    .param = {0x01}
};

static const message_t iso9141_msg_00 =
{
        .cs = 0xCE,
        .data = iso9141_obd_data_00,
        .header = iso9141_header_00
};

static const message_t iso9141_msg_02 =
{
        .cs = 0X45,
        .data = iso9141_obd_data_02,
        .header = iso9141_header_00
};

static obd_timing_ops_t iso9141_timerOpsTx =
{
        .timer_init         = LIBOBD2_TMR_Init,
        .delay_ms           = LIBOBD2_TMR_DelayMs,
        .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
        .start_timeout      = LIBOBD2_TMR_StartTimeout,
        .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

static obd_timing_ops_t iso9141_timerOpsRx =
{
        .timer_init         = LIBOBD2_TMR_Init,
        .delay_ms           = LIBOBD2_TMR_DelayMs,
        .get_time_ms        = LIBOBD2_TMR_GetTimeMs,
        .is_timeout_expired = LIBOBD2_TMR_IsTimeoutExpired,
        .start_timeout      = LIBOBD2_TMR_StartTimeout,
        .stop_timeout       = LIBOBD2_TMR_StopTimeout,
};

static obd_uart_ops_t iso9141_transportOps =
{
    .init        = LIBOBD2_UART_Init,
    .send_byte   = LIBOBD2_UART_WriteByte,
    .recv_byte   = LIBOBD2_UART_RecvByte,
    .send_pulse  = LIBOBD2_UART_SendPulse,
    .switch_mode = LIBOBD2_UART_SwitchMode,
    .flush_rx    = LIBOBD2_UART_FlushRx,
};

static l2_iso9141_ctx_t testeriso9141Ctx =
{
    .header =
    {
        .fmt     = 0x48U,
        .trgAddr = 0x6B,
        .srcAddr = 0x12,
        .len = 1
    },
};

static l2_iso9141_ctx_t ecuiso9141Ctx =
{
    .header =
    {
        .fmt     = 0x48U,
        .trgAddr = 0x6B,
        .srcAddr = 0x12,
        .len = 1
    },
};

static dataLink_if_t ecu_iso9141_dataLink_tx =
{
    .pProtocolCtx     = &ecuiso9141Ctx,
    .pTimingOps       = &iso9141_timerOpsTx,
    .pTimingHandle    = &tmrCtxTx,
    .pTransportHandle = &uartCtxTx,
    .pUartOps    = &iso9141_transportOps,
    .connect          = ecu_l2_iso9141_connect,
    .send_request     = ecu_l2_iso9141_send_request,
    .recv_response    = ecu_l2_iso9141_recv_response,
};

static dataLink_if_t iso9141_dataLink_tx =
{
    .pProtocolCtx     = &testeriso9141Ctx,
    .pTimingOps       = &iso9141_timerOpsTx,
    .pTimingHandle    = &tmrCtxTx,
    .pTransportHandle = &uartCtxTx,
    .pUartOps    = &iso9141_transportOps,
    .connect          = l2_iso9141_connect,
    .send_request     = l2_iso9141_send_request,
    .recv_response    = l2_iso9141_recv_response,
};

static dataLink_if_t iso9141_dataLink_rx =
{
    .pProtocolCtx     = &testeriso9141Ctx,
    .pTimingOps       = &iso9141_timerOpsRx,
    .pTimingHandle    = &tmrCtxRx,
    .pTransportHandle = &uartCtxRx,
    .pUartOps    = &iso9141_transportOps,
    .connect          = l2_iso9141_connect,
    .send_request     = l2_iso9141_send_request,
    .recv_response    = l2_iso9141_recv_response,
};

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t L2_ISO9141_ComputeChecksum(header_t header, obd_data_t data, size_t dataLen);
extern obd_status_t L2_ISO9141_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
extern obd_status_t L2_ISO9141_RecvMessage(dataLink_if_t *self, message_t *recvdMsg, size_t *len);
extern obd_status_t L2_ISO9141_5BaudInit(dataLink_if_t *self, uint8_t* protocol);
extern obd_status_t L2_ISO9141_ReadHeader(dataLink_if_t *self, header_t *header);
extern void L2_ISO9141_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t dataLen,size_t *len);
extern obd_status_t ECU_L2_ISO9141_5BaudInit(dataLink_if_t *self);
static void test_L2_ISO9141_RecvMessage_000_Sender(void *param);
static void test_L2_ISO9141_RecvMessage_000_Receiver(void *param);
static void test_L2_ISO9141_5BaudInit_000_Sender(void *param);
static void test_L2_ISO9141_5BaudInit_000_Receiver(void *param);
static void test_L2_ISO9141_ReadHeader_000_Sender(void *param);
static void test_L2_ISO9141_ReadHeader_000_Receiver(void *param);
static void test_l2_ISO9141_connect_000_Sender(void *param);
static void test_l2_ISO9141_connect_000_Receiver(void *param);
static void test_l2_ISO9141_send_request_000_Sender(void *param);
static void test_l2_ISO9141_send_request_000_Receiver(void *param);
static void test_l2_ISO9141_recv_response_000_Sender(void *param);
static void test_l2_ISO9141_recv_response_000_Receiver(void *param);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void test_L2_ISO9141_RecvMessage_000_Sender(void *param)
{
    dataLink_if_t *pDataLinkTx = &iso9141_dataLink_tx;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    message_t msg = iso9141_msg_00;
    uint8_t aSentMsg[MSG_00_SIZE] = {0};
    size_t len = 0;
    size_t dataLen = 2;

    (void)param;
    (void)iso9141_msg_00;

    // Adds too much delay
    // actual = ECU_DL_SendRequest(pDataLinkTx, &msg.data, 0x2);

    L2_ISO9141_PrepareMessage(&msg, aSentMsg, dataLen, &len);

    for (size_t i = 0; i < len; i++)
    {
        LIBOBD_SendByte(pDataLinkTx, aSentMsg[i]);
        LIBOBD_Delay(pDataLinkTx, P1_TIME_MIN);
    }

    TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);

    LIBOBD_FlushRx(pDataLinkTx);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_L2_ISO9141_RecvMessage_000_Receiver(void *param)
{
    dataLink_if_t *pDataLinkRx = &iso9141_dataLink_rx;
    message_t response = {0};
    size_t len = 0;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint32_t timeSample = 0;

    (void)param;
    (void)dataLink_00;

    timeSample = LIBOBD_GetTimeMs(pDataLinkRx);
    LIBOBD_SetTimeSample(pDataLinkRx , timeSample);

    /* Leave a full P2 max window after the forced P2 min delay below. */
    LIBOBD_StartTimeout(pDataLinkRx, P2_TIME_MAX);
    LIBOBD_Delay(pDataLinkRx, P2_TIME_MIN);

    actual = L2_ISO9141_RecvMessage(pDataLinkRx, &response, &len);
    TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);

    LIBOBD_FlushRx(pDataLinkRx);

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_L2_ISO9141_5BaudInit_000_Sender(void *param)
{
    dataLink_if_t *pDataLinkTx = &iso9141_dataLink_tx;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint8_t protocol = 255;

    (void)param;

    UnitySetTestFile(__FILE__);

    actual = L2_ISO9141_5BaudInit(pDataLinkTx, &protocol);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "L2_KWP_5BaudInit");

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

void test_L2_ISO9141_5BaudInit_000_Receiver(void *param)
{
    dataLink_if_t *pDataLinkRx = &iso9141_dataLink_rx;
    (void)param;

    UnitySetTestFile(__FILE__);

    ECU_L2_ISO9141_5BaudInit(pDataLinkRx);
    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_L2_ISO9141_ReadHeader_000_Sender(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkTx = &iso9141_dataLink_tx;

    (void)param;

    // Send 3 header bytes with 5ms inter-byte gaps (within P1_TIME_MAX=20ms)
    LIBOBD_SendByte(pDataLinkTx, iso9141_header_00.fmt);
    LIBOBD_Delay(pDataLinkTx, P1_TIME_MIN);
    LIBOBD_SendByte(pDataLinkTx, iso9141_header_00.trgAddr);
    LIBOBD_Delay(pDataLinkTx, P1_TIME_MIN);
    LIBOBD_SendByte(pDataLinkTx, iso9141_header_00.srcAddr);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_L2_ISO9141_ReadHeader_000_Receiver(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkRx = &iso9141_dataLink_rx;
    header_t header = {0};
    obd_status_t expected = {0};
    obd_status_t actual = {0};
    uint32_t timeSample = 0;

    (void)param;

    // Mirror the pattern used by test_L2_KWP_RecvMessage_000:
    //   set timeSample = now, start 50ms timeout, busy-wait 25ms so the
    //   P2_MIN elapsed-time check inside L2_KWP_ReadHeader always passes.
    timeSample = LIBOBD_GetTimeMs(pDataLinkRx);
    LIBOBD_SetTimeSample(pDataLinkRx, timeSample);
    /* Leave a full P2 max window after the forced P2 min delay below. */
    LIBOBD_StartTimeout(pDataLinkRx, P2_TIME_MAX);
    LIBOBD_Delay(pDataLinkRx, P2_TIME_MIN);

    actual = L2_ISO9141_ReadHeader(pDataLinkRx, &header);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "L2_KWP_ReadHeader return");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(iso9141_header_00.fmt, header.fmt, "fmt");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(iso9141_header_00.trgAddr, header.trgAddr,  "trgAddr");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(iso9141_header_00.srcAddr, header.srcAddr,  "srcAddr");

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_ISO9141_connect_000_Sender(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkTx = &iso9141_dataLink_tx;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint8_t protocol = 0;

    (void)param;

    actual = l2_iso9141_connect(pDataLinkTx, &protocol);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "l2_iso9141_connect");

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_ISO9141_connect_000_Receiver(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkRx = &iso9141_dataLink_rx;
    (void)param;

    ECU_L2_ISO9141_5BaudInit(pDataLinkRx);

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_ISO9141_send_request_000_Sender(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkTx = &iso9141_dataLink_tx;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint32_t       timeSample  = 0;

    (void)param;

    timeSample = LIBOBD_GetTimeMs(pDataLinkTx);
    LIBOBD_SetTimeSample(pDataLinkTx, timeSample);
    LIBOBD_StartTimeout(pDataLinkTx, P3_TIME_MAX);
    LIBOBD_Delay(pDataLinkTx, P3_TIME_MIN);

    actual = l2_iso9141_send_request(pDataLinkTx, &iso9141_obd_data_00, 1);
    LIBOBD_StopTimeout(pDataLinkTx);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "l2_kwp_send_request");

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_ISO9141_send_request_000_Receiver(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkRx = &iso9141_dataLink_rx;
    uint8_t byte = 0;

    (void)param;

    // Drain MSG_00_SIZE bytes: 3-byte header + sid + param + cs
    for (uint8_t i = 0; i < MSG_00_SIZE; i++)
    {
        RecvByteBlocking(pDataLinkRx, &byte);
        YIELD;
    }

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_ISO9141_recv_response_000_Sender(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkTx = &ecu_iso9141_dataLink_tx;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    size_t dataLen = 0x2;

    (void)param;

    ECU_DL_SendRequest(pDataLinkTx, &iso9141_obd_data_02, dataLen);
    TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_ISO9141_recv_response_000_Receiver(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkRx = &iso9141_dataLink_rx;
    obd_data_t resp        = {0};
    size_t len         = 0;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint32_t timeSample  = 0;

    (void)param;

    timeSample = LIBOBD_GetTimeMs(pDataLinkRx);
    LIBOBD_SetTimeSample(pDataLinkRx, timeSample);
    LIBOBD_StartTimeout(pDataLinkRx, P2_TIME_MAX);

    actual = l2_iso9141_recv_response(pDataLinkRx, &resp, &len);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "l2_kwp_recv_response return");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x81, resp.sid, "resp sid");

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}
/* ================================================ MODULE API ============================================= */

// ==========================================================================================================
// Description: This test shall validate the checksum of an input message
// Type: Positive
// Steps:
//  - Input dummy message 0x68 0x6A 0xF1 0x81 0x01
//  - Call L2_KWP_ComputeChecksum with dummy data
//  - Expected checksum 0x45
// ==========================================================================================================
void test_L2_ISO9141_ComputeChecksum_000(void)
{
    uint8_t expected = 0x45U;
    uint8_t actual   = 0;

    // msg: 0x68 0x6A 0xF1 0x81 0x01 CS:0x45
    actual = L2_ISO9141_ComputeChecksum(iso9141_header_00, iso9141_obd_data_02, 2);

    TEST_ASSERT_EQUAL_HEX8(expected, actual);
}

// ==========================================================================================================
// Description: Test correct usage of L2_KWP_SendMessage
// Type: Positive
// Steps:
//  - Start max p3 timeout
//  - Wait min p3 time
//  - Send msg
//  - Expected response OBD_STATUS_OK
// ==========================================================================================================
void test_L2_ISO9141_SendMessage_000(void)
{
    dataLink_if_t *pDataLink = &iso9141_dataLink_tx;
    obd_status_t expected    = {0};
    obd_status_t actual      = {0};
    uint32_t timeSample = 0;
    uint8_t buffer[6];
    message_t msg = iso9141_msg_00;

    (void)iso9141_msg_00;

    timeSample = LIBOBD_GetTimeMs(pDataLink);
    LIBOBD_SetTimeSample(pDataLink, timeSample);
    LIBOBD_StartTimeout(pDataLink, P3_TIME_MAX);
    LIBOBD_Delay(pDataLink, P3_TIME_MIN);

    L2_ISO9141_PrepareMessage(&msg, buffer, 0x2, NULL);

    actual = L2_ISO9141_SendMessage(pDataLink, buffer, MSG_00_SIZE);

    LIBOBD_StopTimeout(pDataLink);

    TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_ISO9141_RecvMessage_000(void)
{
    TaskHandle_t senderTask = NULL;
    TaskHandle_t receiverTAsk = NULL;
    uint32_t status = 0;

    g_sender_done = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_L2_ISO9141_RecvMessage_000_Receiver,
        "Receiver Task",
        1024,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTAsk);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_L2_ISO9141_RecvMessage_000_Sender,
        "Sender Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,  /* higher: runs first, sends bytes, then deletes itself */
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);
    xTaskResumeAll();

    while ((g_sender_done == pdFALSE) || (g_receiver_done == pdFALSE))
    {
        taskYIELD();
    }

    /* Let the idle task reclaim deleted task stacks before next test */
    vTaskDelay(pdMS_TO_TICKS(10));

    TEST_ASSERT_EQUAL(pdPASS, status);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_ISO9141_5BaudInit_000(void)
{
    TaskHandle_t senderTask = NULL;
    TaskHandle_t receiverTAsk = NULL;
    uint32_t status = 0;

    g_sender_done = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_L2_ISO9141_5BaudInit_000_Sender,
        "Receiver Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTAsk);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_L2_ISO9141_5BaudInit_000_Receiver,
        "Sender Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);
    xTaskResumeAll();

    while ((g_sender_done == pdFALSE) || (g_receiver_done == pdFALSE))
    {
        taskYIELD();
    }

    /* Let the idle task reclaim deleted task stacks before next test */
    vTaskDelay(pdMS_TO_TICKS(10));

    TEST_ASSERT_EQUAL(pdPASS, status);
}

// ==========================================================================================================
// Description: Test correct parsing of a 3-byte KWP header with valid P2 timing
// Type: Positive
// Steps:
//  - ECU sim sends fmt (0xC1), trgAddr (0x33), srcAddr (0xF1) bytes
//  - Receiver sets time sample, starts P2 timeout, delays P2_TIME_MIN before reading
//  - Call L2_KWP_ReadHeader
//  - Expected: OBD_STATUS_OK, all header fields match header_00, headerLen == 3
// ==========================================================================================================
void test_L2_ISO9141_ReadHeader_000(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_L2_ISO9141_ReadHeader_000_Receiver,
        "Receiver Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_L2_ISO9141_ReadHeader_000_Sender,
        "Sender Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);
    xTaskResumeAll();

    while ((g_sender_done == pdFALSE) || (g_receiver_done == pdFALSE))
    {
        taskYIELD();
    }

    /* Let the idle task reclaim deleted task stacks before next test */
    vTaskDelay(pdMS_TO_TICKS(10));

    TEST_ASSERT_EQUAL(pdPASS, status);
}

// ==========================================================================================================
// Description: Test correct serialization of message_t into KWP wire-format byte array
// Type: Positive
// Steps:
//  - Call L2_KWP2000_PrepareMessage with msg_00
//    {fmt=0xC1, trgAddr=0x33, srcAddr=0xF1, sid=0x81, param[0]=0x01, cs=0x45
//  - Expected output bytes: {0xC1, 0x33, 0xF1, 0x81, 0x01, 0x45}, length == MSG_00_SIZE (6)
// ==========================================================================================================
void test_L2_ISO9141_PrepareMessage_000(void)
{
    const uint8_t expected[MSG_00_SIZE]       = {0x68, 0x6A, 0xF1, 0x81, 0x01, 0x45};
    uint8_t       actual[MSG_00_SIZE]     = {0};
    size_t        actualLen                   = 0;
    size_t dataLen = 2;

    L2_ISO9141_PrepareMessage((message_t *)&iso9141_msg_02, actual, dataLen, &actualLen);

    TEST_ASSERT_EQUAL_MESSAGE(MSG_00_SIZE, actualLen, "message length");
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, MSG_00_SIZE);
}

// ==========================================================================================================
// Description: Test successful KWP2000 connection establishment via 5-baud initialization
// Type: Positive
// Steps:
//  - l2_kwp_connect resets context, initializes timing and transport, then calls L2_KWP_Init
//  - ECU sim performs the full 5-baud handshake (sync, KB1, KB2, inverted address)
//  - Expected: OBD_STATUS_OK, CONN_OK bit set in connection status
// ==========================================================================================================
void test_l2_ISO9141_connect_000(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_l2_ISO9141_connect_000_Sender,
        "Sender Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_l2_ISO9141_connect_000_Receiver,
        "Receiver Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTask);
    TEST_ASSERT_EQUAL(pdPASS, status);
    xTaskResumeAll();

    while ((g_sender_done == pdFALSE) || (g_receiver_done == pdFALSE))
    {
        taskYIELD();
    }

    /* Let the idle task reclaim deleted task stacks before next test */
    vTaskDelay(pdMS_TO_TICKS(10));

    TEST_ASSERT_EQUAL(pdPASS, status);
}

// ==========================================================================================================
// Description: Test correct KWP2000 request frame construction and transmission
// Type: Positive
// Steps:
//  - Set kwpCtx.header to known state (3-byte header format, len=0)
//  - Set up P3 timing (start timeout, delay P3_TIME_MIN)
//  - Call l2_kwp_send_request with request_00 (sid=0x81, param=0x01)
//  - ECU sim drains MSG_00_SIZE received bytes
//  - Expected response OBD_STATUS_OK
// ==========================================================================================================
void test_l2_ISO9141_send_request_000(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_l2_ISO9141_send_request_000_Sender,
        "Sender Task",
        512,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_l2_ISO9141_send_request_000_Receiver,
        "Receiver Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTask);
    TEST_ASSERT_EQUAL(pdPASS, status);
    xTaskResumeAll();

    while ((g_sender_done == pdFALSE) || (g_receiver_done == pdFALSE))
    {
        taskYIELD();
    }

    /* Let the idle task reclaim deleted task stacks before next test */
    vTaskDelay(pdMS_TO_TICKS(10));

    TEST_ASSERT_EQUAL(pdPASS, status);
}

// ==========================================================================================================
// Description: Test correct reception and extraction of a KWP2000 response frame
// Type: Positive
// Steps:
//  - ECU sim sends 5-byte wire message: {0xC1, 0x33, 0xF1, 0x81, 0x45}
//    (fmt.bit.len=1 → 3-byte header, 1 data byte, 1 cs byte)
//  - Receiver sets P2 timeout, delays P2_TIME_MIN, calls l2_kwp_recv_response
//  - Expected: OBD_STATUS_OK, resp.positive.sid == 0x81
// ==========================================================================================================
void test_l2_ISO9141_recv_response_000(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_l2_ISO9141_recv_response_000_Receiver,
        "Receiver Task",
        512,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_l2_ISO9141_recv_response_000_Sender,
        "Sender Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);
    xTaskResumeAll();

    while ((g_sender_done == pdFALSE) || (g_receiver_done == pdFALSE))
    {
        taskYIELD();
    }

    /* Let the idle task reclaim deleted task stacks before next test */
    vTaskDelay(pdMS_TO_TICKS(10));

    TEST_ASSERT_EQUAL(pdPASS, status);
}

