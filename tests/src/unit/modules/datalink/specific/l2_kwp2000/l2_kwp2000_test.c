/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "ecu_uart.h"
#include "libobd2.h"
#include "libobd2_test_utils.h"
#include "projdefs.h"
#include <stddef.h>
#include <sys/unistd.h>
#define STM32F4
#include "transport_if.h"
#include "iso15031_5.h"
#include "tusb.h"
#include "uart_kwp_transport_port.h"
#include "unity.h"
#include <stdio.h>
#include <time.h>
#include "kwp_timer.h"
#include "utils.h"
#include "l2_kwp2000.h"

/* ================================================= MACROS ================================================ */
#define MSG_00_SIZE        (0x6U)
/* ============================================ LOCAL VARIABLES ============================================ */
static volatile BaseType_t g_sender_done = pdFALSE;
static volatile BaseType_t g_receiver_done = pdFALSE;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data);
extern obd_status_t L2_KWP_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
extern obd_status_t L2_KWP_RecvMessage(dataLink_if_t *self, message_t *recvdMsg);
#if defined(SPT_FAST_INIT)
extern obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t *self);
extern obd_status_t L2_KWP_SRV_SendData( dataLink_if_t *self, message_t *sentMsg, message_t *recvdMsg);
extern obd_status_t L2_KWP_FastInit(dataLink_if_t *self);
extern void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self);
#endif /* SPT_FAST_INIT */
#if defined(SPT_CHANGE_TIMING_PARAM)
extern obd_status_t L2_KWP_SRV_AccessTimingParameter(dataLink_if_t *self);
#endif /* SPT_CHANGE_TIMING_PARAM */
#if defined(SPT_5BAUD_INIT)
extern obd_status_t L2_KWP_5BaudInit(dataLink_if_t *self);
#endif /* SPT_5BAUD_INIT */
extern obd_status_t L2_KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen);
extern void L2_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len);
static void test_L2_KWP_RecvMessage_000_Sender(void *param);
static void test_L2_KWP_RecvMessage_000_Receiver(void *param);
static void test_L2_KWP_ReadHeader_000_Sender(void *param);
static void test_L2_KWP_ReadHeader_000_Receiver(void *param);
static void test_l2_kwp_connect_000_Sender(void *param);
static void test_l2_kwp_connect_000_Receiver(void *param);
static void test_l2_kwp_send_request_000_Sender(void *param);
static void test_l2_kwp_send_request_000_Receiver(void *param);
static void test_l2_kwp_recv_response_000_Sender(void *param);
static void test_l2_kwp_recv_response_000_Receiver(void *param);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void test_L2_KWP_RecvMessage_000_Sender(void *param)
{
    dataLink_if_t *pDataLinkTx = &dataLink_tx;
    message_t response = {0};
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint8_t aSentMsg[6];
    size_t len;
    message_t msg = msg_00_ecu;

    (void)param;
    (void)msg_00;
    (void)dataLink_00;
    (void)response;

    L2_PrepareMessage(&msg, aSentMsg, &len);

    // ECUSIM_SendMessage(pDataLinkTx, aSentMsg, len);
    TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);

    LIBOBD_FlushRx(pDataLinkTx);

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_L2_KWP_RecvMessage_000_Receiver(void *param)
{
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    message_t response = {0};
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint32_t timeSample = 0;

    (void)msg_00;
    (void)param;
    (void)dataLink_00;

    timeSample = LIBOBD_GetTimeMs(pDataLinkRx);
    LIBOBD_SetTimeSample(pDataLinkRx , timeSample);

    LIBOBD_StartTimeout(pDataLinkRx, P2_TIME_MAX);
    LIBOBD_Delay(pDataLinkRx, P2_TIME_MIN);

    actual = L2_KWP_RecvMessage(pDataLinkRx, &response);
    TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);

    LIBOBD_FlushRx(pDataLinkRx);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_L2_KWP_ReadHeader_000_Sender(void *param)
{
    UnitySetTestFile(__FILE__);
    // dataLink_if_t *pDataLinkTx = &dataLink_tx;

    (void)param;

    // Send 3 header bytes with 5ms inter-byte gaps (within P1_TIME_MAX=20ms)
    // ECUSIM_SendMessage(pDataLinkTx, (uint8_t*)&msg_00, 3);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_L2_KWP_ReadHeader_000_Receiver(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    header_t       header      = {0};
    size_t         headerLen   = 0;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint32_t       timeSample  = 0;
    // uint8_t        stale       = 0;

    (void)param;

    // Mirror the pattern used by test_L2_KWP_RecvMessage_000:
    //   set timeSample = now, start 50ms timeout, busy-wait 25ms so the
    //   P2_MIN elapsed-time check inside L2_KWP_ReadHeader always passes.
    timeSample = LIBOBD_GetTimeMs(pDataLinkRx);
    LIBOBD_SetTimeSample(pDataLinkRx, timeSample);
    LIBOBD_StartTimeout(pDataLinkRx, P2_TIME_MAX);
    LIBOBD_Delay(pDataLinkRx, P2_TIME_MIN);

    actual = L2_KWP_ReadHeader(pDataLinkRx, &header, &headerLen);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "L2_KWP_ReadHeader return");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(header_00.fmt, header.fmt, "fmt");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(header_00.trgAddr, header.trgAddr,  "trgAddr");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(header_00.srcAddr, header.srcAddr,  "srcAddr");
    TEST_ASSERT_EQUAL_MESSAGE(3, headerLen, "headerLen");

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_kwp_connect_000_Sender(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkTx = &dataLink_tx;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint8_t protocol = 0;

    (void)param;

    actual = l2_kwp_connect(pDataLinkTx, &protocol);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "l2_kwp_connect");
    TEST_ASSERT_EQUAL_MESSAGE(1, kwpCtx.conStatus.bits.CONN_OK, "CONN_OK bit");

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_kwp_connect_000_Receiver(void *param)
{
    UnitySetTestFile(__FILE__);

    (void)param;

    //TODO: Reimplement for fast init

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_kwp_send_request_000_Sender(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkTx = &dataLink_tx;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint32_t       timeSample  = 0;

    (void)param;

    timeSample = LIBOBD_GetTimeMs(pDataLinkTx);
    LIBOBD_SetTimeSample(pDataLinkTx, timeSample);
    LIBOBD_StartTimeout(pDataLinkTx, P3_TIME_MAX);
    LIBOBD_Delay(pDataLinkTx, P3_TIME_MIN);

    actual = l2_kwp_send_request(pDataLinkTx, &request_00, 1);
    LIBOBD_StopTimeout(pDataLinkTx);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "l2_kwp_send_request");

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_kwp_send_request_000_Receiver(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    uint8_t        byte        = 0;

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

static void test_l2_kwp_recv_response_000_Sender(void *param)
{
    UnitySetTestFile(__FILE__);
    // dataLink_if_t      *pDataLinkTx = &dataLink_tx;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint8_t aSentMsg[6];
    size_t len;
    message_t msg = msg_00_ecu;

    (void)param;
    (void)msg_00;
    (void)dataLink_00;

    L2_PrepareMessage(&msg, aSentMsg, &len);

    // ECUSIM_SendMessage(pDataLinkTx, aSentMsg, len);
    TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_l2_kwp_recv_response_000_Receiver(void *param)
{
    UnitySetTestFile(__FILE__);
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    obd_response_t resp        = {0};
    size_t         len         = 0;
    obd_status_t expected = {0};
    obd_status_t actual   = {0};
    uint32_t       timeSample  = 0;

    (void)param;

    timeSample = LIBOBD_GetTimeMs(pDataLinkRx);
    LIBOBD_SetTimeSample(pDataLinkRx, timeSample);
    LIBOBD_StartTimeout(pDataLinkRx, P2_TIME_MAX);
    LIBOBD_Delay(pDataLinkRx, P2_TIME_MIN);

    actual = l2_kwp_recv_response(pDataLinkRx, &resp, &len);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "l2_kwp_recv_response return");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x81, resp.positive.sid, "resp sid");

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}
/* ================================================ MODULE API ============================================= */

// ==========================================================================================================
// Description: This test shall validate the checksum of an input message
// Type: Positive
// Steps:
//  - Input dummy message 0xC1 0x33 0xF1 0x81 0x01
//  - Call L2_KWP_ComputeChecksum with dummy data
//  - Expected checksum 0x67
// ==========================================================================================================
void test_L2_KWP_ComputeChecksum_000(void)
{
    uint8_t expected = 0x67U;
    uint8_t actual   = 0;

    // msg: 0xC1 0x33 0xF1 0x81 0x01 CS:0x67
    actual = L2_KWP_ComputeChecksum(header_00, data_00);

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
void test_L2_KWP_SendMessage_000(void)
{
    dataLink_if_t *pDataLink = &dataLink_00;
    obd_status_t expected    = {0};
    obd_status_t actual      = {0};
    uint32_t timeSample = 0;
    uint8_t buffer[6];
    message_t msg = msg_00;

    (void)msg_00;
    (void)dataLink_00;

    timeSample = LIBOBD_GetTimeMs(pDataLink);
    LIBOBD_SetTimeSample(pDataLink, timeSample);
    LIBOBD_StartTimeout(pDataLink, P3_TIME_MAX);
    LIBOBD_Delay(pDataLink, P3_TIME_MIN);

    L2_PrepareMessage(&msg, buffer, NULL);

    actual = L2_KWP_SendMessage(pDataLink, buffer, MSG_00_SIZE);

    LIBOBD_StopTimeout(pDataLink);

    TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);
}

// ==========================================================================================================
// Description: Test L2_KWP_SendMessage with incorrect min p3 time waited
// Type: Negative
// Steps:
//  - Start max p3 timeout
//  - Wait min p3 timeout -1
//  - Send msg
//  - Expected response OBD_ERR_COMM_P3_TIMEOUT_MIN_ECU_TESTER
// ==========================================================================================================
void test_L2_KWP_SendMessage_001(void)
{
    dataLink_if_t *pDataLink = &dataLink_00;
    obd_status_t expected    = {0};
    obd_status_t actual      = {0};
    uint32_t timeSample = 0;

    (void)msg_00;
    (void)dataLink_00;

    {
        timeSample = LIBOBD_GetTimeMs(pDataLink);
        LIBOBD_SetTimeSample(pDataLink, timeSample);
        LIBOBD_StartTimeout(pDataLink, P3_TIME_MAX);
        LIBOBD_Delay(pDataLink, P3_TIME_MAX);

        actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

        LIBOBD_StopTimeout(pDataLink);

        expected.timeout = OBD_ERR_COMM_P3_TIMEOUT_MAX_ECU_TESTER;
        TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);
    }

    {
        timeSample = LIBOBD_GetTimeMs(pDataLink);
        LIBOBD_SetTimeSample(pDataLink, timeSample);
        LIBOBD_StartTimeout(pDataLink, P3_TIME_MAX);
        LIBOBD_Delay(pDataLink, P3_TIME_MAX + 1);

        actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

        LIBOBD_StopTimeout(pDataLink);

        expected.timeout = OBD_ERR_COMM_P3_TIMEOUT_MAX_ECU_TESTER;
        TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);
    }
}

// Description: Test L2_KWP_SendMessage with incorrect max p3 time waited
// Type: Negative
// Steps:
//  - Start max p3 timeout
//  - Wait max p3 timeout + 1
//  - Send msg
//  - Expected response OBD_ERR_COMM_P3_TIMEOUT_MAX_ECU_TESTER
// ==========================================================================================================
void test_L2_KWP_SendMessage_002(void)
{
    dataLink_if_t *pDataLink = &dataLink_00;
    obd_status_t  expected = {0};
    obd_status_t actual = {0};
    uint32_t timeSample = 0;

    (void)msg_00;
    (void)dataLink_00;

    {
        timeSample = LIBOBD_GetTimeMs(pDataLink);
        LIBOBD_SetTimeSample(pDataLink, timeSample);
        LIBOBD_StartTimeout(pDataLink, P3_TIME_MAX);
        LIBOBD_Delay(pDataLink, P3_TIME_MIN - 1);

        actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

        LIBOBD_StopTimeout(pDataLink);

        expected.timeout = OBD_ERR_COMM_P3_TIMEOUT_MIN_ECU_TESTER;
        TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);
    }

    {
        timeSample = LIBOBD_GetTimeMs(pDataLink);
        LIBOBD_SetTimeSample(pDataLink, timeSample);
        LIBOBD_StartTimeout(pDataLink, P3_TIME_MAX);

        actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

        LIBOBD_StopTimeout(pDataLink);

        expected.timeout = OBD_ERR_COMM_P3_TIMEOUT_MIN_ECU_TESTER;
        TEST_ASSERT_EQUAL_OBD_STATUS(expected, actual);
    }
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_RecvMessage_000(void)
{
    TaskHandle_t senderTask = NULL;
    TaskHandle_t receiverTAsk = NULL;
    uint32_t status = 0;

    g_sender_done = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_L2_KWP_RecvMessage_000_Receiver,
        "Receiver Task",
        1024,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTAsk);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_L2_KWP_RecvMessage_000_Sender,
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

#if defined(SPT_FAST_INIT)
// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_SRV_StartCommunication_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_SRV_SendData_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_FastInit_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_IdleBasedOnConnStatus_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

#endif /* SPT_FAST_INIT */
#if defined(SPT_CHANGE_TIMING_PARAM)
// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_SRV_AccessTimingParameter_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

#endif /* SPT_CHANGE_TIMING_PARAM */
#if defined(SPT_5BAUD_INIT)
// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_5BaudInit_000(void)
{
    TaskHandle_t senderTask = NULL;
    TaskHandle_t receiverTAsk = NULL;
    uint32_t status = 0;

    g_sender_done = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_L2_KWP_5BaudInit_000_Sender,
        "Receiver Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTAsk);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_L2_KWP_5BaudInit_000_Receiver,
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

    TEST_ASSERT_EQUAL(pdPASS, status);
}

#endif /* SPT_5BAUD_INIT */

// ==========================================================================================================
// Description: Test correct parsing of a 3-byte KWP header with valid P2 timing
// Type: Positive
// Steps:
//  - ECU sim sends fmt (0xC1), trgAddr (0x33), srcAddr (0xF1) bytes
//  - Receiver sets time sample, starts P2 timeout, delays P2_TIME_MIN before reading
//  - Call L2_KWP_ReadHeader
//  - Expected: OBD_STATUS_OK, all header fields match header_00, headerLen == 3
// ==========================================================================================================
void test_L2_KWP_ReadHeader_000(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_L2_KWP_ReadHeader_000_Sender,
        "Sender Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_L2_KWP_ReadHeader_000_Receiver,
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
// Description: Test correct serialization of message_t into KWP wire-format byte array
// Type: Positive
// Steps:
//  - Call L2_PrepareMessage with msg_00
//    {fmt=0xC1, trgAddr=0x33, srcAddr=0xF1, sid=0x81, param[0]=0x01, cs=0x67}
//  - Expected output bytes: {0xC1, 0x33, 0xF1, 0x81, 0x01, 0x67}, length == MSG_00_SIZE (6)
// ==========================================================================================================
void test_PrepareMessage_000(void)
{
    const uint8_t expected[MSG_00_SIZE]       = {0xC1, 0x33, 0xF1, 0x81, 0x01, 0x67};
    uint8_t       actual[MSG_00_SIZE + 4]     = {0};
    size_t        actualLen                   = 0;

    L2_PrepareMessage((message_t *)&msg_00, actual, &actualLen);

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
void test_l2_kwp_connect_000(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_l2_kwp_connect_000_Sender,
        "Sender Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_l2_kwp_connect_000_Receiver,
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
void test_l2_kwp_send_request_000(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    // Put header in a known state so PrepareMessage produces a 3-byte header (MSG_00_SIZE bytes total)
    kwpCtx.header = (header_t){
        .fmt     = 0xC0,
        .trgAddr = 0x33,
        .srcAddr = 0xF1,
    };

    vTaskSuspendAll();
    status = xTaskCreate(
        test_l2_kwp_send_request_000_Sender,
        "Sender Task",
        512,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_l2_kwp_send_request_000_Receiver,
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
//  - ECU sim sends 5-byte wire message: {0xC1, 0x33, 0xF1, 0x81, 0x67}
//    (fmt.bit.len=1 → 3-byte header, 1 data byte, 1 cs byte)
//  - Receiver sets P2 timeout, delays P2_TIME_MIN, calls l2_kwp_recv_response
//  - Expected: OBD_STATUS_OK, resp.positive.sid == 0x81
// ==========================================================================================================
void test_l2_kwp_recv_response_000(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_l2_kwp_recv_response_000_Sender,
        "Sender Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &senderTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_l2_kwp_recv_response_000_Receiver,
        "Receiver Task",
        512,
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

