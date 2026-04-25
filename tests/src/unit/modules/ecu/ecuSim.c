/* ================================================ INCLUDES =============================================== */
#include "ecuSim.h"
#include "datalink.h"
#include "l2_kwp2000.h"
#include <stdint.h>
#define STM32F4
#include <stddef.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "ecu_uart.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void ECUSIM_SendMessage(dataLink_if_t *pDataLink, uint8_t *pMessage, size_t size)
{
    (void)pDataLink;
    (void)pMessage;

    for (size_t i = 0; i < size; i++)
    {
        LIBOBD_SendByte(pDataLink, pMessage[i]);
        LIBOBD_Delay(pDataLink, P1_TIME_MIN+5);
    }
}

void ECUSIM_RecvMessage(dataLink_if_t *pDataLink, message_t *pMessage)
{
    uint8_t *aMessage = (uint8_t*)pMessage;
    size_t len = 0;
    uint8_t fmt = 0;
    size_t idx = 0;

    // Format byte
    ReadByteInTimeframe(pDataLink, &aMessage[idx], P4_TIME_MIN, P4_TIME_MAX);
    fmt = aMessage[idx];
    len = fmt & 0x3F;

    if (len == 0)
    {
        ReadByteInTimeframe(pDataLink, &aMessage[idx++], P4_TIME_MIN, P4_TIME_MAX);
        ReadByteInTimeframe(pDataLink, &aMessage[idx++], P4_TIME_MIN, P4_TIME_MAX);
        ReadByteInTimeframe(pDataLink, &aMessage[idx++], P4_TIME_MIN, P4_TIME_MAX);
        len = aMessage[3];
    }

    for (; idx < len; idx++)
    {
        ReadByteInTimeframe(pDataLink, &aMessage[idx], P4_TIME_MIN, P4_TIME_MAX);
    }
}
