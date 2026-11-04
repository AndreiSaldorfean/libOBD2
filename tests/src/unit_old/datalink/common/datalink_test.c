/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "datalink_utils.h"
#include "kwp2000.h"
#include "libobd2_test_utils.h"
#include "projdefs.h"
#include "unity.h"
#include <math.h>
#include <stdint.h>
#define STM32F4
#include <stddef.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
static volatile BaseType_t g_sender_done = pdFALSE;
static volatile BaseType_t g_receiver_done = pdFALSE;
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
#if 0 /* TODO: Set a pin on the board as input and measure the time the pin is high/ low */
extern  void SendByteBitBang(dataLink_if_t *self, uint8_t byte, uint8_t baudRate);
#endif

static void test_ReadByteInTimeframe_000_Sender(void *param);
static void test_ReadByteInTimeframe_000_Receiver(void *param);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void test_ReadByteInTimeframe_000_Sender(void* param)
{
    dataLink_if_t *pDataLinkTx = &dataLink_tx;

    (void)param;

    LIBOBD2_TMR_Delay(pDataLinkTx, P2_STAR_TIME_MIN);
    LIBOBD2_UART_SendByte(pDataLinkTx, 0x66);

    LIBOBD2_TMR_Delay(pDataLinkTx, P2_TIME_MIN);
    LIBOBD2_UART_SendByte(pDataLinkTx, 0x66);

    LIBOBD2_TMR_Delay(pDataLinkTx, P1_TIME_MIN);
    LIBOBD2_UART_SendByte(pDataLinkTx, 0x66);

    LIBOBD2_TMR_Delay(pDataLinkTx, P3_TIME_MIN);
    LIBOBD2_UART_SendByte(pDataLinkTx, 0x66);

    LIBOBD2_UART_SendByte(pDataLinkTx, 0x66);

    LIBOBD2_TMR_Delay(pDataLinkTx, P1_TIME_MAX);
    LIBOBD2_UART_SendByte(pDataLinkTx, 0x66);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_ReadByteInTimeframe_000_Receiver(void *param)
{
    dataLink_if_t *handle = &dataLink_rx;
    obd_status_t expStatus = {0};
    obd_status_t status = {0};
    uint8_t byte = 0U;
    uint8_t expected = 0x66U;
    uint8_t actual = 0;
    (void)param;

    status = DL_ReadByteInTimeframe(handle, NULL, &byte, P2_STAR_TIME_MIN, P2_STAR_TIME_MAX);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expStatus , status, "P2 STAR DL_ReadByteInTimeframe");

    status = DL_ReadByteInTimeframe(handle, NULL, &byte, P2_TIME_MIN, P2_TIME_MAX);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expStatus , status, "P2 DL_ReadByteInTimeframe");

    status = DL_ReadByteInTimeframe(handle, NULL, &byte, P1_TIME_MIN, P1_TIME_MAX);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expStatus , status, "P1 DL_ReadByteInTimeframe");

    status = DL_ReadByteInTimeframe(handle, NULL, &byte, P3_TIME_MIN, P3_TIME_MAX);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expStatus , status, "P3 DL_ReadByteInTimeframe");

    expStatus.timeout = OBD_ERR_TIMEOUT_MIN;
    status = DL_ReadByteInTimeframe(handle, NULL, &byte, P3_TIME_MIN, P3_TIME_MAX);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expStatus , status, "P3 DL_ReadByteInTimeframe");

    expStatus.timeout = OBD_ERR_TIMEOUT_MAX;
    status = DL_ReadByteInTimeframe(handle, NULL, &byte, P1_TIME_MIN, P1_TIME_MAX);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expStatus , status, "P3 DL_ReadByteInTimeframe");

    actual = byte;
    TEST_ASSERT_EQUAL_HEX8(expected, actual);

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}
/* ================================================ MODULE API ============================================= */
#if 0 /* TODO: Set a pin on the board as input and measure the time the pin is high/ low */
void test_SendByteBitBang_000(void)
{ }
#endif

void test_ReadByteInTimeframe_000(void)
{
    TaskHandle_t senderTask = NULL;
    TaskHandle_t receiverTAsk = NULL;
    uint32_t status = 0;


    vTaskSuspendAll();
    status = xTaskCreate(
        test_ReadByteInTimeframe_000_Receiver,
        "Receiver Task",
        256,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTAsk);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_ReadByteInTimeframe_000_Sender,
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

void test_RecvByteBlocking_000(void)
{
    dataLink_if_t *pDataLinkTx = &dataLink_tx;
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    uint8_t byte = 0U;
    uint8_t expected = 0x66U;
    uint8_t actual = 0;

    LIBOBD2_UART_SendByte(pDataLinkTx, 0x66);

    DL_RecvByteBlocking(pDataLinkRx, &byte);

    actual = byte;
    TEST_ASSERT_EQUAL_HEX8(expected, actual);
}

