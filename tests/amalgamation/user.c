/* Smoke test for the single header: a TU that only sees the declarations.
 * Wires stub UART / timer ops with no ECU on the line, so LibOBD2_Init must fail. */
#include "libobd2.h"
#include <stdio.h>

static uint32_t fakeTimeMs = 0U;

static bool Uart_Init(void *handle) { (void)handle; return true; }
static obd_uart_rx_status_t Uart_RecvByte(void *handle, uint8_t *buffer) { (void)handle; (void)buffer; return OBD_UART_RX_NO_DATA; }
static void Uart_SendByte(void *handle, uint8_t data) { (void)handle; (void)data; }
static void Uart_SendPulse(void *handle, bool pulse) { (void)handle; (void)pulse; }
static void Uart_SwitchMode(void *handle, uint8_t mode) { (void)handle; (void)mode; }

static obd_tmr_status_t Tmr_Init(void *handle) { (void)handle; return OBD_TMR_OK; }
static uint32_t Tmr_GetTimeMs(void *handle) { (void)handle; return fakeTimeMs++; }
static void Tmr_DelayMs(void *handle, uint32_t delayMs) { (void)handle; fakeTimeMs += delayMs; }
static obd_tmr_status_t Tmr_StartTimeout(void *handle, uint32_t timeoutMs, timing_callback_t callback, void *pUserData)
{
    (void)handle; (void)timeoutMs; (void)callback; (void)pUserData;
    return OBD_TMR_OK;
}
static obd_tmr_status_t Tmr_StopTimeout(void *handle) { (void)handle; return OBD_TMR_OK; }
static bool Tmr_IsTimeoutExpired(void *handle) { (void)handle; return true; }

int main(void)
{
    obd_uart_ops_t uartOps = {
        Uart_Init, Uart_RecvByte, NULL, Uart_SendByte, Uart_SendPulse, Uart_SwitchMode
    };
    obd_timing_ops_t timingOps = {
        Tmr_Init, Tmr_GetTimeMs, Tmr_DelayMs, Tmr_StartTimeout, Tmr_StopTimeout, Tmr_IsTimeoutExpired
    };
    dataLink_if_t dataLink = {0};
    obd_ctx_t ctx = {0};
    obd_status_t status = {0};

    dataLink.pUartOps   = &uartOps;
    dataLink.pTimingOps = &timingOps;
    ctx.pDataLink       = &dataLink;

    status = LibOBD2_Init(&ctx);
    if ((OBD_STATUS_OK == status.response) && (OBD_STATUS_OK == status.timeout))
    {
        printf("FAIL: LibOBD2_Init reported a connection with no ECU\n");
        return 1;
    }

    printf("OK: libobd2 %s, LibOBD2_Init without ECU -> response 0x%04X timeout 0x%04X\n",
           LIBOBD2_VERSION, (unsigned)status.response, (unsigned)status.timeout);
    return 0;
}
