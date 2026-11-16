/* ================================================ INCLUDES =============================================== */
#include "uart_trace.h"
#include "libobd2_uart_port.h"
#include "datalink.h"
#include "datalink_utils.h"
#include "vcd_io.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* ================================================= MACROS ================================================ */
#define TICKS_PER_MS        1000u /* trace timescale is 1 us */
#define SESSION_GAP_MS      5u    /* idle time drawn between two link sessions */
#define SESSION_PULSE_TICKS 10u   /* width of the session pulse */
#define NOTE_HOLD_TICKS     500u  /* how long a note stays visible (extra signals only) */
#define NOTE_MAX_LEN        64u
#define TRACE_PATH_MAX      512u
#define INIT_BAUD           5.0   /* ISO9141 5-baud wake-up address */
#define ROW_COLOR_KLINE     0xFF2E86C1u
#define ROW_COLOR_INIT      0xFFE67E22u

/* ============================================ LOCAL VARIABLES ============================================ */
static vcdWriter_t    *g_writer = NULL;
static vcdUartConfig_t g_lineUart; /* frames on the wire */
static vcdUartConfig_t g_rxUart;   /* frames on rx_data, same baud as the wire */
static vcdTimescale_t  g_lineTimescale;

static vcdSignal_t g_line;
static vcdSignal_t g_rxData;
static vcdSignal_t g_rxWaiting;
static vcdSignal_t g_sessionPulse;
static vcdSignal_t g_uartError;
static vcdSignal_t g_fault;
#if UART_TRACE_EXTRA_SIGNALS
static vcdSignal_t g_txByte;
static vcdSignal_t g_rxByte;
static vcdSignal_t g_trial;
static vcdSignal_t g_note;
#endif

static uint32_t  g_session;   /* number of the current link session */
static bool      g_srcValid;  /* g_srcLast holds a timestamp of this session */
static uint32_t  g_srcLast;   /* last virtual timer value seen (ms) */
static vcdTime_t g_traceLast; /* trace time of g_srcLast (ticks) */
static vcdTime_t g_lineFree;  /* end of the last frame on the wire */
static vcdTime_t g_readFree;  /* end of the last frame on rx_data */
static vcdTime_t g_traceMax;  /* latest time anything was drawn at */
static bool      g_bitbang;   /* between DIGITAL_MODE_BEGIN and DIGITAL_MODE_END */
static bool      g_sawBitbang; /* the trace holds a 5-baud bit-banged byte */
static char      g_path[TRACE_PATH_MAX]; /* where the running trace goes */

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static void touch(vcdTime_t t);
static void noteAt(vcdTime_t t, const char *text);
static void startSession(void);
static vcdTime_t traceTime(uint32_t timeMs);
static bool makeTracePath(const char *name, char *path, size_t size);
static bool makeParentDirs(char *path);
static void writePulseViewSetup(void);
#if UART_TRACE_EXTRA_SIGNALS
static const char *errorName(uint8_t error);
#endif

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static void touch(vcdTime_t t)
{
    if (t > g_traceMax)
    {
        g_traceMax = t;
    }
}

static void noteAt(vcdTime_t t, const char *text)
{
#if UART_TRACE_EXTRA_SIGNALS
    (void)VCD_ChangeString(g_writer, g_note, t, text);
    (void)VCD_ChangeString(g_writer, g_note, t + NOTE_HOLD_TICKS, "-");
    touch(t + NOTE_HOLD_TICKS);
#else
    (void)t;
    (void)text;
#endif
}

/* A new link session starts after everything drawn so far, with a short idle gap. The gap also comes
 * before the first session: a frame starting at time 0 would have no idle-to-start edge to decode. */
static void startSession(void)
{
    vcdTime_t start = (g_traceMax > g_lineFree) ? g_traceMax : g_lineFree;

    start += (vcdTime_t)SESSION_GAP_MS * TICKS_PER_MS;
    g_session++;
    g_srcValid = false;
    g_traceLast = start;
    g_lineFree = start;
    g_readFree = start;
    touch(start + SESSION_PULSE_TICKS);

    (void)VCD_Pulse(g_writer, g_sessionPulse, start, SESSION_PULSE_TICKS);
    (void)VCD_ChangeBit(g_writer, g_rxWaiting, start, '0');
#if UART_TRACE_EXTRA_SIGNALS
    (void)VCD_ChangeU64(g_writer, g_trial, start, g_session);
#endif
    /* nothing earlier than the session start can follow: write it out and free the memory */
    (void)VCD_Commit(g_writer, start);
}

/* Maps a virtual timer value to trace time. The first timestamp of a session lands on the session start,
 * long idle gaps are shortened, and a timer that went backwards (reset without UART_Init) opens
 * a new session. */
static vcdTime_t traceTime(uint32_t timeMs)
{
    /* UART_Init() ran before UART_TRACE_Begin(): open the first session here, otherwise the first
     * edge lands at time 0 on top of the idle level and is lost */
    if (g_session == 0u)
    {
        startSession();
    }

    if (g_srcValid && (timeMs < g_srcLast))
    {
        startSession();
    }

    if (g_srcValid)
    {
        uint32_t delta = timeMs - g_srcLast;
        /* bit-banged 5-baud bits last 200 ms and must keep their real length */
        if ((delta > UART_TRACE_MAX_GAP_MS) && !g_bitbang)
        {
            char text[NOTE_MAX_LEN];
            (void)snprintf(text, sizeof text, "skipped_%lums", (unsigned long)(delta - UART_TRACE_MAX_GAP_MS));
            delta = UART_TRACE_MAX_GAP_MS;
            noteAt(g_traceLast, text);
        }
        g_traceLast += (vcdTime_t)delta * TICKS_PER_MS;
    }
    g_srcLast = timeMs;
    g_srcValid = true;
    touch(g_traceLast);
    return g_traceLast;
}

/* 'name' relative to UART_TRACE_DIR (a leading "./" is dropped); absolute paths are kept as they are */
static bool makeTracePath(const char *name, char *path, size_t size)
{
    int len;

    while ((name[0] == '.') && (name[1] == '/'))
    {
        name += 2;
    }
    if ((name[0] == '/') || (UART_TRACE_DIR[0] == '\0'))
    {
        len = snprintf(path, size, "%s", name);
    }
    else
    {
        len = snprintf(path, size, "%s/%s", UART_TRACE_DIR, name);
    }
    return (len > 0) && ((size_t)len < size);
}

/* mkdir -p for every directory in front of the file name */
static bool makeParentDirs(char *path)
{
    bool ok = true;

    for (char *sep = strchr(path + 1, '/'); ok && (sep != NULL); sep = strchr(sep + 1, '/'))
    {
        *sep = '\0';
        ok = (mkdir(path, 0777) == 0) || (errno == EEXIST);
        *sep = '/';
    }
    return ok;
}

/* <trace>.pvs next to <trace>.vcd: PulseView loads it with the trace and the UART rows come up decoded.
 * The 5-baud row is only added when there is a bit-banged byte to decode, it turns 10400 baud traffic
 * into noise otherwise. */
static void writePulseViewSetup(void)
{
    vcdPulseViewUart_t uarts[2];
    char pvsPath[TRACE_PATH_MAX];
    size_t count = 1u;
    vcdStatus_t st;

    VCD_PulseViewUartFromConfig(&uarts[0], &g_lineUart, g_lineTimescale, "line", "rx_data");
    uarts[0].name  = "K-line (RX: line, TX: rx_data)";
    uarts[0].color = ROW_COLOR_KLINE;
    if (g_sawBitbang)
    {
        VCD_PulseViewUartDefault(&uarts[1], "line", NULL, INIT_BAUD);
        uarts[1].name  = "5-baud init (line)";
        uarts[1].color = ROW_COLOR_INIT;
        count++;
    }

    if (VCD_PulseViewSetupPath(g_path, pvsPath, sizeof pvsPath) >= sizeof pvsPath)
    {
        fprintf(stderr, "uart_trace: path too long for the PulseView setup of %s\n", g_path);
        return;
    }
    st = VCD_PulseViewWriteSetup(pvsPath, uarts, count);
    if (st != VCD_OK)
    {
        fprintf(stderr, "uart_trace: cannot write %s: %s\n", pvsPath, VCD_StatusStr(st));
    }
}

#if UART_TRACE_EXTRA_SIGNALS
static const char *errorName(uint8_t error)
{
    switch (error)
    {
        case OBD_UART_RX_PARITY_ERR:
            return "PARITY_ERROR";
        case OBD_UART_RX_FRAMING_ERR:
            return "FRAMING_ERROR";
        case OBD_UART_RX_OVERRUN_ERR:
            return "OVERRUN_ERROR";
        case OBD_UART_RX_NOISE_ERR:
            return "NOISE_ERROR";
        default:
            return "UART_ERROR";
    }
}
#endif

/* ================================================ MODULE API ============================================= */
bool UART_TRACE_Begin(const char *path)
{
    vcdWriterConfig_t cfg;

    UART_TRACE_End();

    if ((path == NULL) || !makeTracePath(path, g_path, sizeof g_path) || !makeParentDirs(g_path))
    {
        fprintf(stderr, "uart_trace: cannot create the trace %s\n", (path != NULL) ? path : "(null)");
        return false;
    }

    VCD_WriterConfigDefault(&cfg);
    cfg.timescale.magnitude = 1u;
    cfg.timescale.unit = VCD_UNIT_US;
    cfg.order = VCD_ORDER_SORTED; /* frames are drawn ahead of the reads that consume them */
    cfg.comment = "libOBD2 host test: virtual K-line";
    if (VCD_WriterOpenFile(&g_writer, g_path, &cfg) != VCD_OK)
    {
        g_writer = NULL;
        return false;
    }

    (void)VCD_ScopeBegin(g_writer, "kline");
    (void)VCD_AddVar(g_writer, VCD_VAR_WIRE, 1u, "line", &g_line);
    (void)VCD_AddVar(g_writer, VCD_VAR_WIRE, 1u, "rx_data", &g_rxData);
    (void)VCD_AddVar(g_writer, VCD_VAR_WIRE, 1u, "rx_waiting", &g_rxWaiting);
    (void)VCD_AddVar(g_writer, VCD_VAR_WIRE, 1u, "session", &g_sessionPulse);
    (void)VCD_AddVar(g_writer, VCD_VAR_WIRE, 1u, "uart_error", &g_uartError);
    (void)VCD_AddVar(g_writer, VCD_VAR_WIRE, 1u, "fault", &g_fault);
#if UART_TRACE_EXTRA_SIGNALS
    (void)VCD_AddVar(g_writer, VCD_VAR_WIRE, 8u, "tx_byte[7:0]", &g_txByte);
    (void)VCD_AddVar(g_writer, VCD_VAR_WIRE, 8u, "rx_byte[7:0]", &g_rxByte);
    (void)VCD_AddVar(g_writer, VCD_VAR_INTEGER, 32u, "trial", &g_trial);
    (void)VCD_AddVar(g_writer, VCD_VAR_STRING, 0u, "note", &g_note);
#endif
    (void)VCD_ScopeEnd(g_writer);

    g_lineTimescale = cfg.timescale;
    VCD_UartConfigDefault(&g_lineUart, cfg.timescale, (double)UART_TRACE_BAUD);
    VCD_UartConfigDefault(&g_rxUart, cfg.timescale, (double)UART_TRACE_BAUD);
#if UART_TRACE_EXTRA_SIGNALS
    g_lineUart.dataSignal = g_txByte;
    g_rxUart.dataSignal = g_rxByte;
#endif

    g_session = 0u;
    g_srcValid = false;
    g_srcLast = 0u;
    g_traceLast = 0u;
    g_lineFree = 0u;
    g_readFree = 0u;
    g_traceMax = 0u;
    g_bitbang = false;
    g_sawBitbang = false;

    (void)VCD_UartWriteIdle(g_writer, g_line, 0u, &g_lineUart);
    (void)VCD_UartWriteIdle(g_writer, g_rxData, 0u, &g_rxUart);
    (void)VCD_ChangeBit(g_writer, g_rxWaiting, 0u, '0');
    (void)VCD_ChangeBit(g_writer, g_fault, 0u, '0');
    (void)VCD_ChangeBit(g_writer, g_sessionPulse, 0u, '0');
    (void)VCD_ChangeBit(g_writer, g_uartError, 0u, '0');
#if UART_TRACE_EXTRA_SIGNALS
    (void)VCD_ChangeBits(g_writer, g_txByte, 0u, "z");
    (void)VCD_ChangeBits(g_writer, g_rxByte, 0u, "z");
    (void)VCD_ChangeU64(g_writer, g_trial, 0u, 0u);
    (void)VCD_ChangeString(g_writer, g_note, 0u, "-");
#endif
    return true;
}

void UART_TRACE_End(void)
{
    vcdStatus_t st;

    if (g_writer == NULL)
    {
        return;
    }
    st = VCD_WriterClose(g_writer, g_traceMax + TICKS_PER_MS);
    if (st != VCD_OK)
    {
        fprintf(stderr, "uart_trace: trace incomplete: %s\n", VCD_StatusStr(st));
    }
    g_writer = NULL;
    writePulseViewSetup();
}

void UART_TRACE_Note(const char *text)
{
    if ((g_writer == NULL) || (text == NULL))
    {
        return;
    }
    noteAt(g_traceLast, text);
}

void UART_TRACE_OnLinkInit(void)
{
    if (g_writer == NULL)
    {
        return;
    }
    startSession();
}

void UART_TRACE_OnWrite(uint8_t byte, uint32_t timeMs, uint8_t error)
{
    vcdTime_t start;
    vcdTime_t end = 0u;

    if (g_writer == NULL)
    {
        return;
    }

    /* a byte written while the previous frame is still on the wire waits for it, like a UART TX FIFO */
    start = traceTime(timeMs);
    if (start < g_lineFree)
    {
        start = g_lineFree;
    }

    (void)VCD_UartWriteFrame(
        g_writer,
        g_line,
        start,
        byte,
        &g_lineUart,
        (error == OBD_UART_RX_FRAMING_ERR) ? VCD_UART_FLAG_FRAMING_ERROR : 0u,
        &end);
    if (error != OBD_UART_RX_OK)
    {
        /* uart_error is high for the whole frame that carries an injected error */
        (void)VCD_ChangeBit(g_writer, g_uartError, start, '1');
        (void)VCD_ChangeBit(g_writer, g_uartError, end, '0');
#if UART_TRACE_EXTRA_SIGNALS
        noteAt(start, errorName(error));
#endif
    }
    g_lineFree = end;
    touch(end);
}

void UART_TRACE_OnRead(uint8_t byte, uint32_t timeMs, bool received)
{
    vcdTime_t t;

    if (g_writer == NULL)
    {
        return;
    }
    t = traceTime(timeMs);

    if (!received)
    {
        (void)VCD_ChangeBit(g_writer, g_rxWaiting, t, '1');
        return;
    }

    /* reads that come faster than a frame lasts (e.g. several at one timer value with timeStep 0) are
     * drawn back to back, so every frame is complete and decodes at UART_TRACE_BAUD */
    if (t < g_readFree)
    {
        t = g_readFree;
    }

    (void)VCD_ChangeBit(g_writer, g_rxWaiting, t, '0');
    (void)VCD_UartWriteFrame(g_writer, g_rxData, t, byte, &g_rxUart, 0u, &g_readFree);
    touch(g_readFree);
}

void UART_TRACE_OnPulse(bool level, uint32_t timeMs)
{
    vcdTime_t t;

    if (g_writer == NULL)
    {
        return;
    }
    t = traceTime(timeMs);
    if (t < g_lineFree)
    {
        t = g_lineFree;
    }
    (void)VCD_ChangeBool(g_writer, g_line, t, level);
    g_lineFree = t;
}

void UART_TRACE_OnDelay(uint32_t timeMs, uint32_t delayMs)
{
    char text[NOTE_MAX_LEN];

    if (g_writer == NULL)
    {
        return;
    }
    /* the delay is protocol timing (W1..W5), so it is drawn at full length instead of going through the
     * idle-gap shortening in traceTime() */
    (void)snprintf(text, sizeof text, "delay_%lums", (unsigned long)delayMs);
    noteAt(traceTime(timeMs), text);
    g_traceLast += (vcdTime_t)delayMs * TICKS_PER_MS;
    g_srcLast = timeMs + delayMs;
    touch(g_traceLast);
}

void UART_TRACE_OnMode(uint8_t mode, uint32_t timeMs)
{
    vcdTime_t t;
    char text[NOTE_MAX_LEN];

    if (g_writer == NULL)
    {
        return;
    }
    t = traceTime(timeMs);
    switch (mode)
    {
        case DIGITAL_MODE_BEGIN:
            g_bitbang = true;
            g_sawBitbang = true;
            noteAt(t, "BITBANG_BEGIN");
            break;
        case DIGITAL_MODE_END:
            g_bitbang = false;
            noteAt(t, "BITBANG_END");
            break;
        default:
            (void)snprintf(text, sizeof text, "MODE_%u", (unsigned)mode);
            noteAt(t, text);
            break;
    }
}

void UART_TRACE_OnFault(uint32_t timeMs, const char *what)
{
    vcdTime_t t;

    if (g_writer == NULL)
    {
        return;
    }
    t = traceTime(timeMs);
    (void)VCD_Pulse(g_writer, g_fault, t, SESSION_PULSE_TICKS);
    noteAt(t, what);
    touch(t + SESSION_PULSE_TICKS);
}
