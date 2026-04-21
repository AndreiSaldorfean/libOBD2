/* ================================================ INCLUDES =============================================== */
#include "ecuSim.h"
#include "data_link_if.h"
#include "l2_kwp.h"
#include "l2_kwp_utils.h"
#include <stdint.h>
#define STM32F4
#include <stddef.h>
#include <unistd.h>
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"

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

        LIBOBD_Delay(pDataLink, KWP_P1_TIME_MIN);
    }
}

void ECUSIM_RecvMessage(dataLink_if_t *pDataLink, message_t *pMessage)
{
    uint8_t *aMessage = (uint8_t*)pMessage;
    size_t len = 0;
    format_t fmt = {0};
    size_t idx = 0;

    // Format byte
    ReadByteInTimeframe(pDataLink, &aMessage[idx], KWP_P4_TIME_MIN, KWP_P4_TIME_MAX);
    fmt = (format_t)aMessage[idx];
    len = fmt.bit.len;

    if (len == 0)
    {
        ReadByteInTimeframe(pDataLink, &aMessage[idx++], KWP_P4_TIME_MIN, KWP_P4_TIME_MAX);
        ReadByteInTimeframe(pDataLink, &aMessage[idx++], KWP_P4_TIME_MIN, KWP_P4_TIME_MAX);
        ReadByteInTimeframe(pDataLink, &aMessage[idx++], KWP_P4_TIME_MIN, KWP_P4_TIME_MAX);
        len = aMessage[3];
    }

    for (; idx < len; idx++)
    {
        ReadByteInTimeframe(pDataLink, &aMessage[idx], KWP_P4_TIME_MIN, KWP_P4_TIME_MAX);
    }
}
