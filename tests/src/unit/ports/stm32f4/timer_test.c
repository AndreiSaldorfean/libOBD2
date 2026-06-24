/* ================================================ INCLUDES =============================================== */
#include "timer_test.h"
#include "libobd2.h"
#include "libobd2_test_utils.h"
#include "uart_if.h"
#include "iso15031_5.h"
#include "tusb.h"
#include "unity.h"
#include <stdio.h>
#include <time.h>
#include "libobd2_timer_port.h"
#include "utils.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
static volatile BaseType_t g_sender_done = pdFALSE;
static volatile BaseType_t g_receiver_done = pdFALSE;
static uint32_t timeStart = 0;
static uint32_t timeEnd = 0;
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void test_TIMER_2_Sender(void* param)
{
    timerCtx_t *tmr = &tmrCtxTx;
    uint32_t timeElapsed1 = 0;
    uint32_t timeElapsed2 = 0;
    (void)param;

    UnitySetTestFile(__FILE__);

    timeElapsed1 = LIBOBD2_TMR_GetTimeMs(tmr);
    LIBOBD2_TMR_DelayMs(tmr, 10);
    timeElapsed2 = LIBOBD2_TMR_GetTimeMs(tmr);
    YIELD;
    TEST_ASSERT_EQUAL_MESSAGE(10, timeElapsed2 - timeElapsed1, "tx 10ms delay");

    g_sender_done = pdTRUE;
    vTaskDelete(NULL);
}

static void test_TIMER_2_Receiver(void *param)
{
    timerCtx_t *tmr = &tmrCtxRx;
    (void)param;

    UnitySetTestFile(__FILE__);

    timeStart = LIBOBD2_TMR_GetTimeMs(tmr);
    YIELD;
    timeEnd = LIBOBD2_TMR_GetTimeMs(tmr);
    TEST_ASSERT_EQUAL_MESSAGE(10, timeEnd - timeStart, "rx 10ms delay");

    g_receiver_done = pdTRUE;
    vTaskDelete(NULL);
}
/* ================================================ MODULE API ============================================= */
void test_TIMER_0(void)
{
    bool timeoutExpired = true;
    uint32_t timeElapsed1 = 0;
    uint32_t timeElapsed2 = 0;
    (void)timeoutExpired;

    UnitySetTestFile(__FILE__);

    timeElapsed1 = LIBOBD2_TMR_GetTimeMs(&tmrCtxTx);

    LIBOBD2_TMR_DelayMs(&tmrCtxTx, 10);

    timeElapsed2 = LIBOBD2_TMR_GetTimeMs(&tmrCtxTx);

    TEST_ASSERT_EQUAL_MESSAGE(10, timeElapsed2 - timeElapsed1, "10ms delay");

    LIBOBD2_TMR_StartTimeout(&tmrCtxTx, 9, NULL, NULL);

    LIBOBD2_TMR_DelayMs(&tmrCtxTx, 10);

    timeoutExpired = LIBOBD2_TMR_IsTimeoutExpired(&tmrCtxTx);
    TEST_ASSERT_TRUE_MESSAGE(timeoutExpired, "tmr expired");

    LIBOBD2_TMR_StartTimeout(&tmrCtxTx, 9, NULL, NULL);

    LIBOBD2_TMR_StopTimeout(&tmrCtxTx);

    LIBOBD2_TMR_DelayMs(&tmrCtxTx, 10);

    timeoutExpired = LIBOBD2_TMR_IsTimeoutExpired(&tmrCtxTx);
    TEST_ASSERT_FALSE_MESSAGE(timeoutExpired, "tmr not expired");
}

void test_TIMER_1(void)
{
    bool timeoutExpired = true;
    uint32_t timeElapsed1 = 0;
    uint32_t timeElapsed2 = 0;
    (void)timeoutExpired;

    UnitySetTestFile(__FILE__);

    timeElapsed1 = LIBOBD2_TMR_GetTimeMs(&tmrCtxRx);

    LIBOBD2_TMR_DelayMs(&tmrCtxRx, 10);

    timeElapsed2 = LIBOBD2_TMR_GetTimeMs(&tmrCtxRx);

    TEST_ASSERT_EQUAL_MESSAGE(10, timeElapsed2 - timeElapsed1, "10ms delay");

    LIBOBD2_TMR_StartTimeout(&tmrCtxRx, 9, NULL, NULL);

    LIBOBD2_TMR_DelayMs(&tmrCtxRx, 10);

    timeoutExpired = LIBOBD2_TMR_IsTimeoutExpired(&tmrCtxRx);
    TEST_ASSERT_TRUE_MESSAGE(timeoutExpired, "tmr expired");

    LIBOBD2_TMR_StartTimeout(&tmrCtxRx, 9, NULL, NULL);

    LIBOBD2_TMR_StopTimeout(&tmrCtxRx);

    LIBOBD2_TMR_DelayMs(&tmrCtxRx, 10);

    timeoutExpired = LIBOBD2_TMR_IsTimeoutExpired(&tmrCtxRx);
    TEST_ASSERT_FALSE_MESSAGE(timeoutExpired, "tmr not expired");
}

void test_TIMER_2(void)
{
    TaskHandle_t senderTask   = NULL;
    TaskHandle_t receiverTask = NULL;
    uint32_t     status       = 0;

    g_sender_done   = pdFALSE;
    g_receiver_done = pdFALSE;

    vTaskSuspendAll();
    status = xTaskCreate(
        test_TIMER_2_Receiver,
        "Receiver Task",
        512,
        NULL,
        tskIDLE_PRIORITY + 3,
        &receiverTask);
    TEST_ASSERT_EQUAL(pdPASS, status);

    status = xTaskCreate(
        test_TIMER_2_Sender,
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

void test_TIMER_3(void)
{
    bool timeoutExpired = true;
    uint32_t timeElapsed1 = 0;
    uint32_t timeElapsed2 = 0;
    (void)timeoutExpired;

    UnitySetTestFile(__FILE__);

    timeElapsed1 = LIBOBD2_TMR_GetTimeMs(&tmrCtxRx);

    LIBOBD2_TMR_StartTimeout(&tmrCtxRx, 9, NULL, NULL);

    while(!LIBOBD2_TMR_IsTimeoutExpired(&tmrCtxRx));

    timeElapsed2 = LIBOBD2_TMR_GetTimeMs(&tmrCtxRx);

    TEST_ASSERT_EQUAL_MESSAGE(9, timeElapsed2 - timeElapsed1, "9ms delay");
}
