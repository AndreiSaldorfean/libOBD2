/* ================================================ INCLUDES =============================================== */
#include "vcd_pulseview.h"
#include "vcd_internal.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define PV_NAME_MAX         255u /* longest row/signal name accepted */
#define PV_LINE_MAX         (2u * PV_NAME_MAX + 64u)
#define PV_UART_CHANNELS    2u   /* uart decoder: optional channels rx (id 0), tx (id 1) */
#define PV_PIN_SAME_AS_S0   2    /* SRD_INITIAL_PIN_SAME_AS_SAMPLE0 */
#define PV_DEFAULT_NAME     "UART"

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    vcdSinkFn_t sink;
    void       *ctx;
    bool        ok;
} pvOut_t;

/* One decoder option as PulseView stores it: name, GVariant type string, serialized value */
typedef struct
{
    const char *name;
    char        type;  /* 'x' int64, 'd' double, 's' string */
    int64_t     i64;
    double      f64;
    const char *str;
} pvOption_t;

/* ============================================ LOCAL VARIABLES ============================================ */
static const char *const formatNames[] = {"hex", "ascii", "dec", "oct", "bin"};

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void pvPut(pvOut_t *out, const char *text, size_t len);
static void pvPrintf(pvOut_t *out, const char *fmt, ...) VCD_PRINTF_FMT(2, 3);
static bool pvNameValid(const char *name);
static void pvPutIniString(pvOut_t *out, const char *text);
static void pvPutByteArray(pvOut_t *out, const uint8_t *data, size_t len);
static void pvPutOption(pvOut_t *out, unsigned decoder, unsigned idx, const pvOption_t *opt);
static bool pvUartValid(const vcdPulseViewUart_t *uart);
static bool pvSetupValid(const vcdPulseViewUart_t *uarts, size_t count);
static const char *pvParityName(vcdUartParity_t parity);
static double pvStopBits(vcdUartStop_t stop);
static void pvPutUart(pvOut_t *out, unsigned idx, const vcdPulseViewUart_t *uart);
static bool fileSink(void *ctx, const char *data, size_t len);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void pvPut(pvOut_t *out, const char *text, size_t len)
{
    if (out->ok && (len > 0u))
    {
        out->ok = out->sink(out->ctx, text, len);
    }
}

static void pvPrintf(pvOut_t *out, const char *fmt, ...)
{
    char line[PV_LINE_MAX];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
    if ((len < 0) || ((size_t)len >= sizeof line))
    {
        out->ok = false;
        return;
    }
    pvPut(out, line, (size_t)len);
}

/* Printable ASCII only: Qt reads the INI file as Latin-1, and the name must match PulseView's byte for byte */
static bool pvNameValid(const char *name)
{
    size_t len = 0u;

    if (name == NULL)
    {
        return false;
    }
    for (; name[len] != '\0'; len++)
    {
        if (((unsigned char)name[len] < 0x20u) || ((unsigned char)name[len] > 0x7Eu))
        {
            return false;
        }
    }
    return (len > 0u) && (len <= PV_NAME_MAX);
}

/* A QSettings INI string: '"' and '\' escaped, quoted when it holds ; , = or edge spaces */
static void pvPutIniString(pvOut_t *out, const char *text)
{
    size_t len = strlen(text);
    bool quote = (strpbrk(text, ";,=") != NULL) || (len == 0u) || (text[0] == ' ') || (text[len - 1u] == ' ');
    size_t i;

    if (quote)
    {
        pvPut(out, "\"", 1u);
    }
    for (i = 0u; i < len; i++)
    {
        if ((text[i] == '"') || (text[i] == '\\'))
        {
            pvPut(out, "\\", 1u);
        }
        pvPut(out, &text[i], 1u);
    }
    if (quote)
    {
        pvPut(out, "\"", 1u);
    }
}

/* QByteArray value: every byte as a \xHH escape. Each escape is followed by '\' or ')', neither a hex digit,
 * so Qt's greedy hex parser stops at the right place. */
static void pvPutByteArray(pvOut_t *out, const uint8_t *data, size_t len)
{
    size_t i;

    pvPut(out, "@ByteArray(", 11u);
    for (i = 0u; i < len; i++)
    {
        pvPrintf(out, "\\x%02x", (unsigned)data[i]);
    }
    pvPut(out, ")", 1u);
}

/* GVariant serialization: int64/double are 8 bytes in host order, a string is its bytes plus NUL */
static void pvPutOption(pvOut_t *out, unsigned decoder, unsigned idx, const pvOption_t *opt)
{
    uint8_t raw[8];

    pvPrintf(out, "decoder%u\\option%u\\name=%s\n", decoder, idx, opt->name);
    pvPrintf(out, "decoder%u\\option%u\\type=%c\n", decoder, idx, opt->type);
    pvPrintf(out, "decoder%u\\option%u\\value=", decoder, idx);
    switch (opt->type)
    {
        case 'x':
            memcpy(raw, &opt->i64, sizeof raw);
            pvPutByteArray(out, raw, sizeof raw);
            break;
        case 'd':
            memcpy(raw, &opt->f64, sizeof raw);
            pvPutByteArray(out, raw, sizeof raw);
            break;
        default:
            pvPutByteArray(out, (const uint8_t *)opt->str, strlen(opt->str) + 1u);
            break;
    }
    pvPut(out, "\n", 1u);
}

static bool pvUartValid(const vcdPulseViewUart_t *uart)
{
    bool channelsOk = ((uart->rx != NULL) || (uart->tx != NULL))
                   && ((uart->rx == NULL) || pvNameValid(uart->rx))
                   && ((uart->tx == NULL) || pvNameValid(uart->tx));

    return channelsOk
        && ((uart->name == NULL) || pvNameValid(uart->name))
        && isfinite(uart->baud) && (uart->baud > 0.0) && (uart->baud < 1e12)
        && (uart->dataBits >= 5u) && (uart->dataBits <= 9u)
        && ((unsigned)uart->parity <= (unsigned)VCD_UART_PARITY_SPACE)
        && ((unsigned)uart->stopBits <= (unsigned)VCD_UART_STOP_2)
        && ((unsigned)uart->format <= (unsigned)VCD_PV_FORMAT_BIN);
}

static bool pvSetupValid(const vcdPulseViewUart_t *uarts, size_t count)
{
    size_t i;

    if ((uarts == NULL) || (count == 0u) || (count > VCD_PV_MAX_DECODERS))
    {
        return false;
    }
    for (i = 0u; i < count; i++)
    {
        if (!pvUartValid(&uarts[i]))
        {
            return false;
        }
    }
    return true;
}

static const char *pvParityName(vcdUartParity_t parity)
{
    switch (parity)
    {
        case VCD_UART_PARITY_ODD:
            return "odd";
        case VCD_UART_PARITY_EVEN:
            return "even";
        case VCD_UART_PARITY_MARK:
            return "one";
        case VCD_UART_PARITY_SPACE:
            return "zero";
        default:
            return "none";
    }
}

static double pvStopBits(vcdUartStop_t stop)
{
    switch (stop)
    {
        case VCD_UART_STOP_1_5:
            return 1.5;
        case VCD_UART_STOP_2:
            return 2.0;
        default:
            return 1.0;
    }
}

/* [decode_signal<idx>]: one decode signal holding a single uart decoder */
static void pvPutUart(pvOut_t *out, unsigned idx, const vcdPulseViewUart_t *uart)
{
    const char *invert = uart->inverted ? "yes" : "no";
    const pvOption_t options[] = {
        {"baudrate", 'x', (int64_t)floor(uart->baud + 0.5), 0.0, NULL},
        {"data_bits", 'x', (int64_t)uart->dataBits, 0.0, NULL},
        {"parity", 's', 0, 0.0, pvParityName(uart->parity)},
        {"stop_bits", 'd', 0, pvStopBits(uart->stopBits), NULL},
        {"bit_order", 's', 0, 0.0, uart->msbFirst ? "msb-first" : "lsb-first"},
        {"format", 's', 0, 0.0, formatNames[uart->format]},
        {"invert_rx", 's', 0, 0.0, invert},
        {"invert_tx", 's', 0, 0.0, invert},
    };
    const char *channels[PV_UART_CHANNELS] = {uart->rx, uart->tx};
    const char *channelNames[PV_UART_CHANNELS] = {"RX", "TX"};
    unsigned i;

    pvPrintf(out, "\n[decode_signal%u]\n", idx);
    pvPut(out, "name=", 5u);
    pvPutIniString(out, (uart->name != NULL) ? uart->name : PV_DEFAULT_NAME);
    pvPut(out, "\nenabled=true\n", 14u);
    if (uart->color != 0u)
    {
        pvPrintf(out, "color=%lu\n", (unsigned long)uart->color);
    }

    pvPut(out, "decoders=1\n", 11u);
    pvPut(out, "decoder0\\id=uart\n", 17u);
    pvPut(out, "decoder0\\visible=true\n", 22u);
    pvPrintf(out, "decoder0\\options=%u\n", (unsigned)(sizeof options / sizeof options[0]));
    for (i = 0u; i < (sizeof options / sizeof options[0]); i++)
    {
        pvPutOption(out, 0u, i, &options[i]);
    }

    pvPrintf(out, "channels=%u\n", PV_UART_CHANNELS);
    for (i = 0u; i < PV_UART_CHANNELS; i++)
    {
        pvPrintf(out, "channel%u\\name=%s\n", i, channelNames[i]);
        pvPrintf(out, "channel%u\\initial_pin_state=%d\n", i, PV_PIN_SAME_AS_S0);
        if (channels[i] != NULL)
        {
            pvPrintf(out, "channel%u\\assigned_signal_name=", i);
            pvPutIniString(out, channels[i]);
            pvPut(out, "\n", 1u);
        }
    }
}

static bool fileSink(void *ctx, const char *data, size_t len)
{
    return fwrite(data, 1u, len, (FILE *)ctx) == len;
}

/* ================================================ MODULE API ============================================= */
void VCD_PulseViewUartDefault(vcdPulseViewUart_t *uart, const char *rx, const char *tx, double baud)
{
    if (uart == NULL)
    {
        return;
    }
    memset(uart, 0, sizeof *uart);
    uart->rx = rx;
    uart->tx = tx;
    uart->baud = baud;
    uart->dataBits = 8u;
    uart->parity = VCD_UART_PARITY_NONE;
    uart->stopBits = VCD_UART_STOP_1;
    uart->format = VCD_PV_FORMAT_HEX;
}

void VCD_PulseViewUartFromConfig(
    vcdPulseViewUart_t *uart,
    const vcdUartConfig_t *cfg,
    vcdTimescale_t ts,
    const char *rx,
    const char *tx)
{
    double tick = VCD_TimescaleSeconds(ts);
    double baud = ((cfg != NULL) && (cfg->bitTicks > 0.0) && (tick > 0.0)) ? 1.0 / (cfg->bitTicks * tick) : 0.0;

    VCD_PulseViewUartDefault(uart, rx, tx, baud);
    if ((uart == NULL) || (cfg == NULL))
    {
        return;
    }
    uart->dataBits = cfg->dataBits;
    uart->parity = cfg->parity;
    uart->stopBits = cfg->stopBits;
    uart->msbFirst = cfg->msbFirst;
    uart->inverted = cfg->inverted;
}

size_t VCD_PulseViewSetupPath(const char *vcdPath, char *out, size_t outSize)
{
    const char *base;
    const char *dot;
    size_t stem;
    int len;

    if (vcdPath == NULL)
    {
        vcdPath = "";
    }
    /* only a dot in the file name counts, not one in a directory name ("./a" or "dir.d/a") */
    base = strrchr(vcdPath, '/');
    base = (base != NULL) ? base + 1 : vcdPath;
    dot = strrchr(base, '.');
    stem = ((dot != NULL) && (dot != base)) ? (size_t)(dot - vcdPath) : strlen(vcdPath);

    len = snprintf(out, (out != NULL) ? outSize : 0u, "%.*s.pvs", (int)stem, vcdPath);
    return (len < 0) ? 0u : (size_t)len;
}

vcdStatus_t VCD_PulseViewWriteSetupSink(
    vcdSinkFn_t sink,
    void *ctx,
    const vcdPulseViewUart_t *uarts,
    size_t count)
{
    pvOut_t out = {sink, ctx, true};
    size_t i;

    if ((sink == NULL) || !pvSetupValid(uarts, count))
    {
        return VCD_ERR_ARG;
    }

    pvPut(&out, "[General]\n", 10u);
    pvPrintf(&out, "decode_signals=%u\n", (unsigned)count);
    pvPut(&out, "meta_objs=0\n", 12u);
    for (i = 0u; i < count; i++)
    {
        pvPutUart(&out, (unsigned)i, &uarts[i]);
    }
    return out.ok ? VCD_OK : VCD_ERR_IO;
}

vcdStatus_t VCD_PulseViewWriteSetup(const char *path, const vcdPulseViewUart_t *uarts, size_t count)
{
    FILE *file;
    vcdStatus_t st;

    /* checked before the file is created, so a bad call leaves an existing setup alone */
    if ((path == NULL) || !pvSetupValid(uarts, count))
    {
        return VCD_ERR_ARG;
    }
    file = fopen(path, "wb");
    if (file == NULL)
    {
        return VCD_ERR_IO;
    }
    st = VCD_PulseViewWriteSetupSink(fileSink, file, uarts, count);
    if ((fclose(file) != 0) && (st == VCD_OK))
    {
        st = VCD_ERR_IO;
    }
    if (st != VCD_OK)
    {
        (void)remove(path);
    }
    return st;
}
