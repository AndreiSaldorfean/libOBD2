/* ================================================ INCLUDES =============================================== */
#include "tasks.h"
#include "libobd2_test.h"
#include "libobd2_test_utils.h"
#include "timer_test.h"
#include "uart_kwp_transport_port.h"
#include "uart_test.h"
#include "unity.h"
#include "unity_internals.h"
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
#include "l2_iso9141_test.h"
#include "l2_iso9141.h"
#include "l2_kwp2000_test.h"
#include "datalink_test.h"
#include "task.h"
#include "uart_test.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void DL_Utils_TestTask(void *param)
{
    (void)param;

    RUN_TEST(test_ReadByteInTimeframe_000);
    RUN_TEST(test_RecvByteBlocking_000);
}

void L2_ISO9141_TestTask(void *param)
{
    (void)param;
    printf("============= UNIT BEGIN ==============\n");
    UNITY_BEGIN();

    RUN_TEST(test_L2_ISO9141_ComputeChecksum_000);
    RUN_TEST(test_L2_ISO9141_SendMessage_000);
    RUN_TEST(test_L2_ISO9141_RecvMessage_000);
    RUN_TEST(test_L2_ISO9141_ReadHeader_000);
    RUN_TEST(test_L2_ISO9141_PrepareMessage_000);
    RUN_TEST(test_L2_ISO9141_5BaudInit_000);
    RUN_TEST(test_l2_ISO9141_connect_000);
    RUN_TEST(test_l2_ISO9141_send_request_000);
    RUN_TEST(test_l2_ISO9141_recv_response_000);

    UNITY_END();
    vTaskDelete(NULL);
}

void L2_KWP_TestTask(void *param)
{
    (void)param;
    printf("============= UNIT BEGIN ==============\n");
    UNITY_BEGIN();


    RUN_TEST(test_L2_KWP_ComputeChecksum_000);
    RUN_TEST(test_L2_KWP_SendMessage_000);
    RUN_TEST(test_L2_KWP_SendMessage_001);
    RUN_TEST(test_L2_KWP_SendMessage_002);
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
    RUN_TEST(test_L2_KWP_ReadHeader_000);
    RUN_TEST(test_PrepareMessage_000);
    RUN_TEST(test_l2_kwp_connect_000);
    RUN_TEST(test_l2_kwp_send_request_000);
    RUN_TEST(test_l2_kwp_recv_response_000);

    UNITY_END();
    vTaskDelete(NULL);
}

void LIBOBD2_TestTask(void *param)
{
    (void)param;

    UNITY_BEGIN();

    // RUN_TEST(test_LibOBD2_Init_000);
    RUN_TEST(test_LibOBD2_RequestService_000);

    UNITY_END();
    vTaskDelete(NULL);
}

void UART_TestTask(void *param)
{
    (void)param;

    UNITY_BEGIN();

    RUN_TEST(test_UART_000);

    UNITY_END();
    vTaskDelete(NULL);
}
