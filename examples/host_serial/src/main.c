/*************************************************************************************************************
 * obd2_reader: connects to a car through a serial / USB K-line adapter (e.g. a KKL / FTDI cable) and reads
 * a few OBD2 services with libOBD2 over the host_serial port.
 *
 *   usage: obd2_reader [--list] [--loop] [port]     (default port: /dev/ttyUSB0)
 *************************************************************************************************************/
/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "iso9141.h"
#include "kwp2000.h"
#include "libobd2.h"
#include "libobd2_services.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include "libserialport.h"
#include "statusRetCodes.h"
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define DEFAULT_PORT      "/dev/ttyUSB0"
#define RESPONSE_BUF_LEN  (64U)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* The data link tries every protocol on the same context, so it must fit any of them */
typedef union
{
    iso9141_ctx_t iso9141;
    kwp_ctx_t kwp2000;
}protocolCtx_t;

/* ============================================ LOCAL VARIABLES ============================================ */
static serialUartCtx_t uartCtx =
{
    .portName = DEFAULT_PORT,
    .baudRate = SERIAL_UART_DEFAULT_BAUD,
};

static serialTmrCtx_t tmrCtx;

static obd_uart_ops_t uartOps =
{
    .init        = UART_Init,
    .send_byte   = UART_WriteByte,
    .recv_byte   = UART_RecvByte,
    .send_pulse  = UART_SendPulse,
    .switch_mode = UART_SwitchMode,
    .flush_rx    = UART_FlushRx,
};

static obd_timing_ops_t tmrOps =
{
    .timer_init         = TMR_Init,
    .get_time_ms        = TMR_GetTimeMs,
    .delay_ms           = TMR_DelayMs,
    .start_timeout      = TMR_StartTimeout,
    .stop_timeout       = TMR_StopTimeout,
    .is_timeout_expired = TMR_IsTimeoutExpired,
};

static protocolCtx_t protoCtx;

static dataLink_if_t dataLink =
{
    .pUartOps         = &uartOps,
    .pTransportHandle = &uartCtx,
    .pTimingOps       = &tmrOps,
    .pTimingHandle    = &tmrCtx,
    .pProtocolCtx     = &protoCtx,
    .connect          = ISO9141_Connect,
    .send_request     = ISO9141_SendRequest,
    .recv_response    = ISO9141_RecvResponse,
};

static obd_ctx_t obdCtx =
{
    .pDataLink        = &dataLink,
    .connectionStatus = false,
};

/* Set by SIGINT so --loop can stop and close the port */
static volatile sig_atomic_t stopRequested = 0;

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void onSigint(int sig);
static int listPorts(void);
static bool isOk(obd_status_t status);
static void printStatus(const char *name, obd_status_t status);
static void printText(const char *name, obd_status_t status, const uint8_t *data, size_t len);
static bool readCoolant(void);
static void usage(const char *prog);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void onSigint(int sig)
{
    (void)sig;
    stopRequested = 1;
}

static int listPorts(void)
{
    struct sp_port **ports = NULL;
    int ret = 1;

    if (SP_OK == sp_list_ports(&ports))
    {
        for (size_t idx = 0; NULL != ports[idx]; idx++)
        {
            const char *desc = sp_get_port_description(ports[idx]);
            printf("%s\t%s\n", sp_get_port_name(ports[idx]), (NULL != desc) ? desc : "");
        }
        if (NULL == ports[0])
        {
            printf("no serial ports found\n");
        }
        sp_free_port_list(ports);
        ret = 0;
    }

    return ret;
}

static bool isOk(obd_status_t status)
{
    return (OBD_STATUS_OK == status.response) && (OBD_STATUS_OK == status.timeout);
}

static void printStatus(const char *name, obd_status_t status)
{
    printf("%-13s failed (response=0x%04x timeout=0x%04x)\n", name, status.response, status.timeout);
}

/* VIN / CALID: the ECU pads the text with 0x00, print only the characters */
static void printText(const char *name, obd_status_t status, const uint8_t *data, size_t len)
{
    if (!isOk(status))
    {
        printStatus(name, status);
        return;
    }

    printf("%-13s ", name);
    for (size_t idx = 0; idx < len; idx++)
    {
        if (isprint(data[idx]))
        {
            putchar(data[idx]);
        }
    }
    printf("\n");
}

static bool readCoolant(void)
{
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};
    int16_t temp = 0;

    status = LibOBD2_RequestService(&obdCtx, OBD_GET_COOLANT_TEMP, response, &responseLen);
    if (isOk(status) && (sizeof(temp) == responseLen))
    {
        /* decoded as A - 40, an int16_t */
        memcpy(&temp, response, sizeof(temp));
        printf("%-13s %d degC\n", "Coolant temp", (int)temp);
    }
    else
    {
        printStatus("Coolant temp", status);
    }

    return isOk(status);
}

static void usage(const char *prog)
{
    printf("usage: %s [--list] [--loop] [port]\n"
           "  port    serial device of the K-line adapter (default " DEFAULT_PORT ")\n"
           "  --list  list the serial ports and exit\n"
           "  --loop  after the one-shot reads, poll the coolant temperature every second (Ctrl-C stops)\n",
           prog);
}

/* ================================================ MODULE API ============================================= */
int main(int argc, char **argv)
{
    uint8_t response[RESPONSE_BUF_LEN] = {0};
    size_t responseLen = 0;
    obd_status_t status = {0};
    bool loop = false;
    int ret = 1;

    for (int arg = 1; arg < argc; arg++)
    {
        if (0 == strcmp(argv[arg], "--list"))
        {
            return listPorts();
        }
        else if (0 == strcmp(argv[arg], "--loop"))
        {
            loop = true;
        }
        else if ('-' == argv[arg][0])
        {
            usage(argv[0]);
            return (0 == strcmp(argv[arg], "--help")) ? 0 : 1;
        }
        else
        {
            uartCtx.portName = argv[arg];
        }
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    (void)signal(SIGINT, onSigint);

    printf("Connecting on %s (5-baud init takes ~2.5 s)...\n", uartCtx.portName);
    status = LibOBD2_Init(&obdCtx);
    if (!isOk(status))
    {
        fprintf(stderr, "Connection failed: response=0x%04x timeout=0x%04x\n", status.response, status.timeout);
        if (NULL == uartCtx.port)
        {
            fprintf(stderr, "Could not open %s (try --list, and check you are in the dialout group)\n",
                    uartCtx.portName);
        }
        else
        {
            fprintf(stderr, "No ECU answered the ISO9141 5-baud init: check the ignition is on and the adapter "
                            "is wired to OBD pin 7 (K-line). KWP2000 fast init is not implemented yet.\n");
        }
        goto exit;
    }
    printf("Connected.\n");

    status = LibOBD2_RequestService(&obdCtx, OBD_GET_VIN, response, &responseLen);
    printText("VIN", status, response, responseLen);

    memset(response, 0, sizeof(response));
    responseLen = 0;
    status = LibOBD2_RequestService(&obdCtx, OBD_GET_CALID, response, &responseLen);
    printText("CALID", status, response, responseLen);

    (void)readCoolant();

    /* Requests at least every second also keep the link alive (P3 max is 5 s) */
    while (loop && !stopRequested)
    {
        TMR_DelayMs(&tmrCtx, 1000U);
        if (!stopRequested)
        {
            (void)readCoolant();
        }
    }

    ret = 0;
exit:
    UART_Deinit(&uartCtx);
    return ret;
}
