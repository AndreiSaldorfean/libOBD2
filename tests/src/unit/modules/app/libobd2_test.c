/* ================================================ INCLUDES =============================================== */
#include "libobd2_test.h"
#include "datalink.h"
#include "ecu_uart.h"
#include "l2_iso9141.h"
#include "l2_kwp2000.h"
#include "l2_kwp2000_test.h"
#include "libobd2.h"
#include "libobd2_test_utils.h"
#include "statusRetCodes.h"
#include "unity.h"
#include <stddef.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
static volatile BaseType_t g_sender_done = pdFALSE;
static volatile BaseType_t g_receiver_done = pdFALSE;

/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern obd_status_t LibOBD2_Init(obd_ctx_t *ctx);
obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    libobd2_sid_t obdSid,
    uint8_t *response,
    size_t* responseLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void test_LibOBD2_Init_000_Sender(void *param)
{
    obd_status_t expected = {0};
    obd_status_t actual   = {0};

    (void)param;

    UnitySetTestFile(__FILE__);

    actual = LibOBD2_Init(&ctx);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "LibOBD2_Init");

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

// NOTE: Currently only iso9141 is supported so no need for special handling
static void test_LibOBD2_Init_000_Receiver(void *param)
{
    dataLink_if_t *pDataLinkRx = &dataLink_rx;
    obd_status_t  expected = {0};
    obd_status_t actual = {0};
    uint8_t syncByte    = 0;
    uint8_t kb2Inverted = 0;

    (void)param;

    UnitySetTestFile(__FILE__);

    // Read wake-up byte at 5 baudRate
    TEST_ASSERT_TRUE_MESSAGE(ECUSIM_ReadByteBitBanged(pDataLinkRx, &syncByte, 5), "ECUSIM_ReadByteBitBanged");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x33, syncByte, "sync byte");
    LIBOBD_Delay(pDataLinkRx, ISO9141_W1_TIME_MIN);

    // Send sync byte
    LIBOBD_SendByte(pDataLinkRx, 0x55);
    LIBOBD_FlushRx(pDataLinkRx);

    // Send KB1
    YIELD;
    LIBOBD_Delay(pDataLinkRx, ISO9141_W2_TIME_MIN);
    LIBOBD_SendByte(pDataLinkRx, 0x08);
    LIBOBD_FlushRx(pDataLinkRx);

    // Send KB2
    LIBOBD_SendByte(pDataLinkRx, 0x08);
    LIBOBD_FlushRx(pDataLinkRx);

    // Receive kb2 inverted
    actual = ReadByteInTimeframe(pDataLinkRx, &kb2Inverted, 0, ISO9141_W4_TIME_MAX+100);
    TEST_ASSERT_EQUAL_OBD_STATUS_MESSAGE(expected, actual, "ReadByteInTimeframe: kb2 inverted");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(~0x08, kb2Inverted, "Inverted keybyte");

    // Send inverted address
    YIELD;
    LIBOBD_Delay(pDataLinkRx, ISO9141_W4_TIME_MIN);
    LIBOBD_SendByte(pDataLinkRx, ~0x33);

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}

/* ================================================ MODULE API ============================================= */
void test_LibOBD2_Init_000(void)
{
    TaskHandle_t senderTask = NULL;
    TaskHandle_t receiverTAsk = NULL;
    uint32_t status = 0;

    g_sender_done = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_LibOBD2_Init_000_Sender,
        "Receiver Task",
        512,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTAsk);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_LibOBD2_Init_000_Receiver,
        "Sender Task",
        512,
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

