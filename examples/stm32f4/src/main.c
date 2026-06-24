/* ================================================ INCLUDES =============================================== */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "init.h"
#include "tasks.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
int main(void)
{
    sysInit();

#if 1
    TaskHandle_t ecuTaskHandle = NULL;
    uint32_t status = xTaskCreate(
        EcuTask,
        "Ecu Task",
        1024,
        NULL,
        tskIDLE_PRIORITY,
        &ecuTaskHandle );

    if (status != pdPASS)
    {
        while (1)
            ;
    }
#else
    TaskHandle_t testerTaskHandle = NULL;
    uint32_t status = xTaskCreate(
        TesterTask,
        "Tester Task",
        1024,
        NULL,
        tskIDLE_PRIORITY,
        &testerTaskHandle);

    if (status != pdPASS)
    {
        while (1)
            ;
    }
#endif

    vTaskStartScheduler();

    while (1)
        ;
}
