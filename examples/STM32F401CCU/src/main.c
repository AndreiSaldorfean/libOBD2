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

    TaskHandle_t gHelloWorld = NULL;

    // uint32_t status = xTaskCreate(
    //     DummyTransciever,
    //     "Dummy Transciever Task",
    //     1024,
    //     NULL,
    //     tskIDLE_PRIORITY,
    //     &gHelloWorld);

    uint32_t status = xTaskCreate(
        TransceiverTask,
        "Transceiver Task",
        1024,
        NULL,
        tskIDLE_PRIORITY,
        &gHelloWorld);


    if (status != pdPASS)
    {
        while (1)
            ;
    }

    vTaskStartScheduler();

    while (1)
        ;
}
