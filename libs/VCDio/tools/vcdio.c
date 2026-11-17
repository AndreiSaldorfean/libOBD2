/*************************************************************************************************************
 * vcdio - command line companion of the VCDio library.
 *
 *   vcdio info <file.vcd>                       header, signals and change counts
 *   vcdio dump <file.vcd> <signal>              every value change of one signal
 *   vcdio uart <file.vcd> <signal> <baud> [options]
 *       -d <5..9>          data bits (default 8)
 *       -p <n|e|o|m|s>     parity none/even/odd/mark/space (default n)
 *       -s <1|1.5|2>       stop bits (default 1)
 *       -m                 MSB first
 *       -i                 inverted line (idle low)
 *
 * Exit code: 0 ok, 1 usage error, 2 file/signal error, 3 decoded frames with errors (uart).
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "vcd_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    vcdTimescale_t ts;
    vcdTime_t      prevEnd;
    bool           hasPrev;
    unsigned       dataBits;
    size_t         frames;
    size_t         bad;
} uartPrint_t;

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void usage(void)
{
    fprintf(stderr,
            "usage: vcdio info <file.vcd>\n"
            "       vcdio dump <file.vcd> <signal>\n"
            "       vcdio uart <file.vcd> <signal> <baud> [-d bits] [-p n|e|o|m|s] [-s 1|1.5|2] [-m] [-i]\n");
}

/* Prints seconds with an engineering unit, e.g. "1.234 ms". */
static void formatSeconds(char *buf, size_t size, double seconds)
{
    static const struct
    {
        double      scale;
        const char *unit;
    } units[] = {{1.0, "s"}, {1e-3, "ms"}, {1e-6, "us"}, {1e-9, "ns"}, {1e-12, "ps"}};
    size_t i;

    for (i = 0u; i < (sizeof units / sizeof units[0]) - 1u; i++)
    {
        if ((seconds >= units[i].scale) || (seconds <= 0.0))
        {
            break;
        }
    }
    (void)snprintf(buf, size, "%.3f %s", seconds / units[i].scale, units[i].unit);
}

static vcdReader_t *openOrReport(const char *path)
{
    vcdReader_t *r = NULL;
    vcdParseError_t err;
    vcdStatus_t st = VCD_ReaderOpenFile(&r, path, &err);

    if (st != VCD_OK)
    {
        if (err.line > 0u)
        {
            fprintf(stderr, "%s:%zu: %s\n", path, err.line, err.message);
        }
        else
        {
            fprintf(stderr, "%s: %s\n", path, (err.message[0] != '\0') ? err.message : VCD_StatusStr(st));
        }
    }
    return r;
}

static bool findOrReport(const vcdReader_t *r, const char *name, size_t *var)
{
    vcdStatus_t st = VCD_ReaderFind(r, name, var);

    if (st == VCD_ERR_ARG)
    {
        fprintf(stderr, "signal '%s' is ambiguous, use the full name (vcdio info lists them)\n", name);
    }
    else if (st != VCD_OK)
    {
        fprintf(stderr, "no signal '%s' (vcdio info lists them)\n", name);
    }
    return st == VCD_OK;
}

static int cmdInfo(const char *path)
{
    vcdReader_t *r = openOrReport(path);
    vcdTimescale_t ts;
    char start[32];
    char end[32];
    size_t i;

    if (r == NULL)
    {
        return 2;
    }
    ts = VCD_ReaderTimescale(r);
    formatSeconds(start, sizeof start, VCD_TicksToSeconds(ts, VCD_ReaderStartTime(r)));
    formatSeconds(end, sizeof end, VCD_TicksToSeconds(ts, VCD_ReaderEndTime(r)));

    printf("file       %s\n", path);
    if (VCD_ReaderDate(r) != NULL)
    {
        printf("date       %s\n", VCD_ReaderDate(r));
    }
    if (VCD_ReaderVersion(r) != NULL)
    {
        printf("version    %s\n", VCD_ReaderVersion(r));
    }
    if (VCD_ReaderComment(r) != NULL)
    {
        printf("comment    %s\n", VCD_ReaderComment(r));
    }
    printf("timescale  %u %s\n", (unsigned)ts.magnitude, VCD_UnitStr(ts.unit));
    printf("time       %s .. %s\n\n", start, end);

    printf("%-40s %-9s %6s %-6s %10s\n", "signal", "type", "width", "id", "changes");
    for (i = 0u; i < VCD_ReaderVarCount(r); i++)
    {
        const vcdVarInfo_t *v = VCD_ReaderVar(r, i);
        printf("%-40s %-9s %6u %-6s %10zu\n", v->fullName, v->typeName, (unsigned)v->width, v->id,
               VCD_ReaderChangeCount(r, i));
    }
    VCD_ReaderClose(r);
    return 0;
}

static int cmdDump(const char *path, const char *name)
{
    vcdReader_t *r = openOrReport(path);
    vcdTimescale_t ts;
    size_t var;
    size_t i;

    if (r == NULL)
    {
        return 2;
    }
    if (!findOrReport(r, name, &var))
    {
        VCD_ReaderClose(r);
        return 2;
    }
    ts = VCD_ReaderTimescale(r);
    printf("%20s  %14s  %s\n", "ticks", "time", "value");
    for (i = 0u; i < VCD_ReaderChangeCount(r, var); i++)
    {
        vcdTime_t t = 0u;
        const char *v = NULL;
        char when[32];

        (void)VCD_ReaderChange(r, var, i, &t, &v);
        formatSeconds(when, sizeof when, VCD_TicksToSeconds(ts, t));
        printf("%20llu  %14s  %s\n", (unsigned long long)t, when, v);
    }
    VCD_ReaderClose(r);
    return 0;
}

static bool printFrame(void *ctx, const vcdUartFrame_t *frame)
{
    uartPrint_t *p = (uartPrint_t *)ctx;
    char when[32];
    char gap[32];
    char status[48] = "";
    int width = (int)((p->dataBits + 3u) / 4u);
    int c = (int)(frame->data & 0xFFu);

    formatSeconds(when, sizeof when, VCD_TicksToSeconds(p->ts, frame->start));
    if (p->hasPrev && (frame->start >= p->prevEnd))
    {
        formatSeconds(gap, sizeof gap, VCD_TicksToSeconds(p->ts, frame->start - p->prevEnd));
    }
    else
    {
        (void)snprintf(gap, sizeof gap, "-");
    }

    if (frame->errors == 0u)
    {
        (void)snprintf(status, sizeof status, "ok");
    }
    else
    {
        if ((frame->errors & VCD_UART_ERR_FRAMING) != 0u)
        {
            (void)strcat(status, "framing ");
        }
        if ((frame->errors & VCD_UART_ERR_PARITY) != 0u)
        {
            (void)strcat(status, "parity ");
        }
        if ((frame->errors & VCD_UART_ERR_UNKNOWN) != 0u)
        {
            (void)strcat(status, "unknown-level ");
        }
        p->bad++;
    }

    printf("%14s  %14s    0x%0*X  %c    %s\n", when, gap, width, (unsigned)frame->data,
           ((c >= 0x20) && (c < 0x7F) && (p->dataBits <= 8u)) ? c : '.', status);

    p->prevEnd = frame->end;
    p->hasPrev = true;
    p->frames++;
    return true;
}

static int cmdUart(int argc, char **argv)
{
    const char *path = argv[2];
    const char *name = argv[3];
    double baud = strtod(argv[4], NULL);
    vcdUartConfig_t cfg;
    uartPrint_t print;
    vcdReader_t *r;
    size_t var;
    int i;

    if (!(baud > 0.0))
    {
        fprintf(stderr, "invalid baud rate '%s'\n", argv[4]);
        return 1;
    }

    r = openOrReport(path);
    if (r == NULL)
    {
        return 2;
    }
    VCD_UartConfigDefault(&cfg, VCD_ReaderTimescale(r), baud);

    for (i = 5; i < argc; i++)
    {
        const char *opt = argv[i];
        const char *val = (i + 1 < argc) ? argv[i + 1] : NULL;

        if (strcmp(opt, "-m") == 0)
        {
            cfg.msbFirst = true;
        }
        else if (strcmp(opt, "-i") == 0)
        {
            cfg.inverted = true;
        }
        else if ((strcmp(opt, "-d") == 0) && (val != NULL))
        {
            cfg.dataBits = (uint8_t)atoi(val);
            i++;
        }
        else if ((strcmp(opt, "-p") == 0) && (val != NULL))
        {
            static const char letters[] = "noems";
            const char *at = strchr(letters, val[0]);
            if ((at == NULL) || (val[0] == '\0'))
            {
                fprintf(stderr, "invalid parity '%s'\n", val);
                VCD_ReaderClose(r);
                return 1;
            }
            cfg.parity = (vcdUartParity_t)(at - letters);
            i++;
        }
        else if ((strcmp(opt, "-s") == 0) && (val != NULL))
        {
            cfg.stopBits = (strcmp(val, "2") == 0) ? VCD_UART_STOP_2
                           : (strcmp(val, "1.5") == 0) ? VCD_UART_STOP_1_5
                                                       : VCD_UART_STOP_1;
            i++;
        }
        else
        {
            usage();
            VCD_ReaderClose(r);
            return 1;
        }
    }

    if (!findOrReport(r, name, &var))
    {
        VCD_ReaderClose(r);
        return 2;
    }

    memset(&print, 0, sizeof print);
    print.ts = VCD_ReaderTimescale(r);
    print.dataBits = cfg.dataBits;

    printf("# %s  %s  %.0f baud  %u%c%s%s%s\n", path, VCD_ReaderVar(r, var)->fullName, baud,
           (unsigned)cfg.dataBits, "NOEMS"[cfg.parity],
           (cfg.stopBits == VCD_UART_STOP_2) ? "2" : (cfg.stopBits == VCD_UART_STOP_1_5) ? "1.5" : "1",
           cfg.msbFirst ? "  msb-first" : "", cfg.inverted ? "  inverted" : "");
    printf("%14s  %14s    %-6s %-4s %s\n", "start", "gap", "data", "chr", "status");

    if (VCD_UartDecode(r, var, &cfg, printFrame, &print) != VCD_OK)
    {
        fprintf(stderr, "cannot decode '%s': it must be a 1-bit signal and the settings valid\n", name);
        VCD_ReaderClose(r);
        return 2;
    }
    printf("# %zu frames, %zu with errors\n", print.frames, print.bad);
    VCD_ReaderClose(r);
    return (print.bad == 0u) ? 0 : 3;
}

/* ================================================ ENTRY POINT ============================================ */
int main(int argc, char **argv)
{
    if ((argc == 3) && (strcmp(argv[1], "info") == 0))
    {
        return cmdInfo(argv[2]);
    }
    if ((argc == 4) && (strcmp(argv[1], "dump") == 0))
    {
        return cmdDump(argv[2], argv[3]);
    }
    if ((argc >= 5) && (strcmp(argv[1], "uart") == 0))
    {
        return cmdUart(argc, argv);
    }
    usage();
    return 1;
}
