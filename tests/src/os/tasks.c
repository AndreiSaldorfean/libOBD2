/* ================================================ INCLUDES =============================================== */
#include "tasks.h"
#include "libobd2_test_utils.h"
#include "timer_test.h"
#include "uart_kwp_transport_port.h"
#include "unity.h"
#include "unity_internals.h"
#include "test_libobd2.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"

#include <stdint.h>
#define STM32F4
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/cm3/nvic.h"
#include "tusb.h"
#include "l2_kwp_test.h"
#include "l2_kwp_utils_test.h"
#include "task.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void TestTask(void *param)
{
    (void)param;
    printf("============= UNIT BEGIN ==============\n");

    UNITY_BEGIN();

    /* TODO: Add proper unit tests and turn examples module to proper examples not bad integration tests */
    #if 0
    RUN_TEST(test_LIBOBD2_0);
    RUN_TEST(test_LIBOBD2_1);
    RUN_TEST(test_TIMER_0);
    RUN_TEST(test_TIMER_1);
    #endif

    /* ======================= Unit tests ======================= */

    /* ---------------------  l2_kwp_utils ---------------------- */
    RUN_TEST(test_ReadByteInTimeframe_000);
    RUN_TEST(test_RecvByteBlocking_000);
    /* ---------------------  l2_kwp_utils ---------------------- */

    /* ------------------------  l2_kwp ------------------------- */
    RUN_TEST(test_L2_KWP_ComputeChecksum_000);
    // RUN_TEST(test_L2_KWP_SendMessage_000);
    // RUN_TEST(test_L2_KWP_SendMessage_001);
    // RUN_TEST(test_L2_KWP_SendMessage_002);
    RUN_TEST(test_L2_KWP_RecvMessage_000);
#if defined(SPT_FAST_INIT)
    RUN_TEST(test_L2_KWP_SRV_StartCommunication_000);
    RUN_TEST(test_L2_KWP_SRV_SendData_000);
    RUN_TEST(test_L2_KWP_FastInit_000);
    RUN_TEST(test_L2_KWP_IdleBasedOnConnStatus_000);
#endif /* SPT_FAST_INIT */
#if defined(SPT_CHANGE_TIMING_PARAM)
    RUN_TEST(test_L2_KWP_SRV_AccessTimingParameter_000);
#endif /* SPT_CHANGE_TIMING_PARAM */
#if defined(SPT_5BAUD_INIT)
    RUN_TEST(test_L2_KWP_5BaudInit_000);
#endif /* SPT_5BAUD_INIT */
    RUN_TEST(test_L2_KWP_Init_000);
    RUN_TEST(test_L2_KWP_ReadHeader_000);
    RUN_TEST(test_PrepareMessage_000);
    /* ------------------------  l2_kwp ------------------------- */

    /* ======================= Unit tests ======================= */

    UNITY_END();

    while(true)
    {
        #if !defined(DEBUG)
        tud_cdc_write_flush();
        tud_task();
        #endif /* DEBUG */
    }

}
