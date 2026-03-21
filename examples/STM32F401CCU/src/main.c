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

    TaskHandle_t testerTaskHandle = NULL;

    /* === Normal Transceiver (client) === */
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

    vTaskStartScheduler();

    while (1)
        ;
}
