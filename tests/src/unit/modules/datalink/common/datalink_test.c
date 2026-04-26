/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "l2_kwp2000.h"
#include "libobd2_test_utils.h"
#include "projdefs.h"
#include "unity.h"
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

extern obd_status_t ReadByteInTimeframe(dataLink_if_t *self, uint8_t *byte, uint16_t timeMin, uint16_t timeMax);
extern obd_status_t RecvByteBlocking(dataLink_if_t *self, uint8_t *byte);

static void test_ReadByteInTimeframe_000_Sender(void *param);
static void test_ReadByteInTimeframe_000_Receiver(void *param);
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void test_ReadByteInTimeframe_000_Sender(void* param)
{
    dataLink_if_t *pDataLinkTx = &dataLink_tx;

    (void)param;

    LIBOBD_Delay(pDataLinkTx, P2_STAR_TIME_MIN);

    LIBOBD_SendByte(pDataLinkTx, 0x66);

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_ReadByteInTimeframe_000_Receiver(void *param)
{
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    obd_status_t status = {0};
    uint8_t byte = 0U;
    uint8_t expected = 0x66U;
    uint8_t actual = 0;
    (void)param;

    status = ReadByteInTimeframe(pDataLinkRx, &byte, P2_STAR_TIME_MIN, P2_STAR_TIME_MAX);
    TEST_ASSERT_EQUAL_HEX16(OBD_STATUS_OK, status.response);

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

    LIBOBD_SendByte(pDataLinkTx, 0x66);

    RecvByteBlocking(pDataLinkRx, &byte);

    actual = byte;
    TEST_ASSERT_EQUAL_HEX8(expected, actual);
}

