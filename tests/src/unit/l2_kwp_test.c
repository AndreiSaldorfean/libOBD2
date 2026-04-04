/* ================================================ INCLUDES =============================================== */
#include "data_link_if.h"
#include "libobd2.h"
#include "libobd2_test_utils.h"
#include "projdefs.h"
#define STM32F4
#include "srv_status.h"
#include "transport_if.h"
#include "iso15031_5.h"
#include "tusb.h"
#include "uart_kwp_transport_port.h"
#include "unity.h"
#include <stdio.h>
#include <time.h>
#include "kwp_timer.h"
#include "utils.h"
#include "l2_kwp.h"

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
extern obd_status_t L2_KWP_Init(dataLink_if_t *self);
extern obd_status_t L2_KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen);
extern void L2_KWP_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len);



/* TODO: Create a tasks_tests for tester-> ecu tests */

void test_L2_KWP_RecvMessage_000_Sender(void *param)
{
    dataLink_if_t *pDataLinkTx = &dataLink_tx;
    message_t response = {0};
    obd_status_t  expected = OBD_STATUS_OK;
    obd_status_t actual = 0;
    // uint32_t timeSample = 0;

    (void)param;
    (void)msg_00;
    (void)dataLink_00;
    (void)response;

    // timeSample = LIBOBD_GetTimeMs(pDataLinkTx);
    // LIBOBD_SetTimeSample(pDataLinkTx, timeSample);
    // LIBOBD_StartTimeout(pDataLinkTx, KWP_P3_TIME_MAX);
    // LIBOBD_Delay(pDataLinkTx, KWP_P3_TIME_MIN);

    actual = L2_KWP_SendMessage(pDataLinkTx, (uint8_t*)&msg_00, MSG_00_SIZE);
    TEST_ASSERT_EQUAL_HEX16(expected, actual);

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

void test_L2_KWP_RecvMessage_000_Receiver(void *param)
{
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    message_t response = {0};
    obd_status_t  expected = OBD_STATUS_OK;
    obd_status_t actual = 0;
    uint32_t timeSample = 0;

    (void)msg_00;
    (void)param;
    (void)dataLink_00;
    (void)timeSample;

    actual = L2_KWP_RecvMessage(pDataLinkRx, &response);
    TEST_ASSERT_EQUAL_HEX16(expected, actual);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

/* TODO: Create a tasks_tests for tester-> ecu tests */

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
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
    obd_status_t  expected = OBD_STATUS_OK;
    obd_status_t actual = 0;
    uint32_t timeSample = 0;

    (void)msg_00;
    (void)dataLink_00;

    timeSample = LIBOBD_GetTimeMs(pDataLink);
    LIBOBD_SetTimeSample(pDataLink, timeSample);
    LIBOBD_StartTimeout(pDataLink, KWP_P3_TIME_MAX);
    LIBOBD_Delay(pDataLink, KWP_P3_TIME_MIN);

    actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

    LIBOBD_StopTimeout(pDataLink);

    TEST_ASSERT_EQUAL_HEX16(expected, actual);
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
    obd_status_t  expected = OBD_STATUS_OK;
    obd_status_t actual = 0;
    uint32_t timeSample = 0;

    (void)msg_00;
    (void)dataLink_00;

    {
        timeSample = LIBOBD_GetTimeMs(pDataLink);
        LIBOBD_SetTimeSample(pDataLink, timeSample);
        LIBOBD_StartTimeout(pDataLink, KWP_P3_TIME_MAX);
        LIBOBD_Delay(pDataLink, KWP_P3_TIME_MAX);

        actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

        LIBOBD_StopTimeout(pDataLink);

        expected = OBD_ERR_COMM_P3_TIMEOUT_MAX_ECU_TESTER;
        TEST_ASSERT_EQUAL_HEX16(expected, actual);
    }

    {
        timeSample = LIBOBD_GetTimeMs(pDataLink);
        LIBOBD_SetTimeSample(pDataLink, timeSample);
        LIBOBD_StartTimeout(pDataLink, KWP_P3_TIME_MAX);
        LIBOBD_Delay(pDataLink, KWP_P3_TIME_MAX + 1);

        actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

        LIBOBD_StopTimeout(pDataLink);

        expected = OBD_ERR_COMM_P3_TIMEOUT_MAX_ECU_TESTER;
        TEST_ASSERT_EQUAL_HEX16(expected, actual);
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
    obd_status_t  expected = OBD_STATUS_OK;
    obd_status_t actual = 0;
    uint32_t timeSample = 0;

    (void)msg_00;
    (void)dataLink_00;

    {
        timeSample = LIBOBD_GetTimeMs(pDataLink);
        LIBOBD_SetTimeSample(pDataLink, timeSample);
        LIBOBD_StartTimeout(pDataLink, KWP_P3_TIME_MAX);
        LIBOBD_Delay(pDataLink, KWP_P3_TIME_MIN - 1);

        actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

        LIBOBD_StopTimeout(pDataLink);

        expected = OBD_ERR_COMM_P3_TIMEOUT_MIN_ECU_TESTER;
        TEST_ASSERT_EQUAL_HEX16(expected, actual);
    }

    {
        timeSample = LIBOBD_GetTimeMs(pDataLink);
        LIBOBD_SetTimeSample(pDataLink, timeSample);
        LIBOBD_StartTimeout(pDataLink, KWP_P3_TIME_MAX);

        actual = L2_KWP_SendMessage(pDataLink, (uint8_t*)&msg_00, MSG_00_SIZE);

        LIBOBD_StopTimeout(pDataLink);

        expected = OBD_ERR_COMM_P3_TIMEOUT_MIN_ECU_TESTER;
        TEST_ASSERT_EQUAL_HEX16(expected, actual);
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


    vTaskSuspendAll();
    status = xTaskCreate(
        test_L2_KWP_RecvMessage_000_Receiver,
        "Receiver Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTAsk);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_L2_KWP_RecvMessage_000_Sender,
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
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

#endif /* SPT_5BAUD_INIT */
// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_Init_000(void)
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
void test_L2_KWP_ReadHeader_000(void)
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
void test_PrepareMessage_000(void)
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
void test_l2_kwp_connect_000(void)
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
void test_l2_kwp_send_request_000(void)
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
void test_l2_kwp_recv_response_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

