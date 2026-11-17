/* ================================================ INCLUDES =============================================== */
#include "vcd_uart.h"
#include <math.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define MIN_DATA_BITS 5u
#define MAX_DATA_BITS 9u
#define UNKNOWN_LEVEL '?'

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static bool configValid(const vcdUartConfig_t *cfg);
static double stopLength(vcdUartStop_t stop);
static vcdTime_t bitEdge(vcdTime_t start, double bitTicks, double bitPos);
static unsigned parityBit(vcdUartParity_t parity, uint16_t data);
static char sampleLevel(const vcdReader_t *r, size_t var, vcdTime_t start, double bitTicks, unsigned bit);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static bool configValid(const vcdUartConfig_t *cfg)
{
    return (cfg != NULL) && isfinite(cfg->bitTicks) && (cfg->bitTicks >= 1.0) &&
           (cfg->dataBits >= MIN_DATA_BITS) && (cfg->dataBits <= MAX_DATA_BITS) &&
           ((unsigned)cfg->parity <= (unsigned)VCD_UART_PARITY_SPACE) &&
           ((unsigned)cfg->stopBits <= (unsigned)VCD_UART_STOP_2);
}

static double stopLength(vcdUartStop_t stop)
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

/* Time of position 'bitPos' (in bits, may be fractional) inside a frame. Always computed from the frame
 * start, so rounding never accumulates. */
static vcdTime_t bitEdge(vcdTime_t start, double bitTicks, double bitPos)
{
    return start + (vcdTime_t)floor((bitPos * bitTicks) + 0.5);
}

static unsigned parityBit(vcdUartParity_t parity, uint16_t data)
{
    unsigned ones = 0u;
    uint16_t d;

    for (d = data; d != 0u; d &= (uint16_t)(d - 1u))
    {
        ones++;
    }
    switch (parity)
    {
        case VCD_UART_PARITY_ODD:
            return (ones & 1u) ^ 1u;
        case VCD_UART_PARITY_EVEN:
            return ones & 1u;
        case VCD_UART_PARITY_MARK:
            return 1u;
        default:
            return 0u;
    }
}

/* Line level in the middle of bit 'bit' of the frame starting at 'start'. */
static char sampleLevel(const vcdReader_t *r, size_t var, vcdTime_t start, double bitTicks, unsigned bit)
{
    const char *v = VCD_ReaderValueAt(r, var, bitEdge(start, bitTicks, (double)bit + 0.5));
    return (v != NULL) ? v[0] : UNKNOWN_LEVEL;
}

/* ================================================ MODULE API ============================================= */
double VCD_UartBitTicks(vcdTimescale_t ts, double baud)
{
    double tick = VCD_TimescaleSeconds(ts);

    if (!(baud > 0.0) || !(tick > 0.0))
    {
        return 0.0;
    }
    return 1.0 / (baud * tick);
}

void VCD_UartConfigDefault(vcdUartConfig_t *cfg, vcdTimescale_t ts, double baud)
{
    if (cfg == NULL)
    {
        return;
    }
    memset(cfg, 0, sizeof *cfg);
    cfg->bitTicks = VCD_UartBitTicks(ts, baud);
    cfg->dataBits = 8u;
    cfg->parity = VCD_UART_PARITY_NONE;
    cfg->stopBits = VCD_UART_STOP_1;
    cfg->msbFirst = false;
    cfg->inverted = false;
    cfg->dataSignal = VCD_SIGNAL_NONE;
}

vcdTime_t VCD_UartFrameTicks(const vcdUartConfig_t *cfg)
{
    double bits;

    if (!configValid(cfg))
    {
        return 0u;
    }
    bits = 1.0 + (double)cfg->dataBits + ((cfg->parity != VCD_UART_PARITY_NONE) ? 1.0 : 0.0) +
           stopLength(cfg->stopBits);
    return bitEdge(0u, cfg->bitTicks, bits);
}

vcdStatus_t VCD_UartWriteIdle(vcdWriter_t *writer, vcdSignal_t line, vcdTime_t time, const vcdUartConfig_t *cfg)
{
    if (cfg == NULL)
    {
        return VCD_ERR_ARG;
    }
    return VCD_ChangeBit(writer, line, time, cfg->inverted ? '0' : '1');
}

vcdStatus_t VCD_UartWriteFrame(
    vcdWriter_t *writer,
    vcdSignal_t line,
    vcdTime_t start,
    uint16_t data,
    const vcdUartConfig_t *cfg,
    uint32_t flags,
    vcdTime_t *end)
{
    char mark;
    char space;
    unsigned pos;
    unsigned i;
    vcdTime_t frameEnd;
    vcdStatus_t st;

    if (!configValid(cfg))
    {
        return VCD_ERR_ARG;
    }
    if (((unsigned)data >> cfg->dataBits) != 0u)
    {
        return VCD_ERR_VALUE;
    }
    mark = cfg->inverted ? '0' : '1';
    space = cfg->inverted ? '1' : '0';

    /* start bit */
    st = VCD_ChangeBit(writer, line, start, space);
    if ((st == VCD_OK) && (cfg->dataSignal != VCD_SIGNAL_NONE))
    {
        st = VCD_ChangeU64(writer, cfg->dataSignal, start, data);
    }
    pos = 1u;

    /* data bits */
    for (i = 0u; (i < cfg->dataBits) && (st == VCD_OK); i++, pos++)
    {
        unsigned index = cfg->msbFirst ? (cfg->dataBits - 1u - i) : i;
        bool one = (((unsigned)data >> index) & 1u) != 0u;
        st = VCD_ChangeBit(writer, line, bitEdge(start, cfg->bitTicks, pos), one ? mark : space);
    }

    /* parity bit */
    if ((st == VCD_OK) && (cfg->parity != VCD_UART_PARITY_NONE))
    {
        unsigned p = parityBit(cfg->parity, data);
        if ((flags & VCD_UART_FLAG_PARITY_ERROR) != 0u)
        {
            p ^= 1u;
        }
        st = VCD_ChangeBit(writer, line, bitEdge(start, cfg->bitTicks, pos), (p != 0u) ? mark : space);
        pos++;
    }

    /* stop bits */
    frameEnd = bitEdge(start, cfg->bitTicks, (double)pos + stopLength(cfg->stopBits));
    if (st == VCD_OK)
    {
        bool broken = (flags & VCD_UART_FLAG_FRAMING_ERROR) != 0u;
        st = VCD_ChangeBit(writer, line, bitEdge(start, cfg->bitTicks, pos), broken ? space : mark);
        if ((st == VCD_OK) && broken)
        {
            st = VCD_ChangeBit(writer, line, frameEnd, mark);
        }
    }
    if ((st == VCD_OK) && (cfg->dataSignal != VCD_SIGNAL_NONE))
    {
        st = VCD_ChangeBits(writer, cfg->dataSignal, frameEnd, "z");
    }

    if ((st == VCD_OK) && (end != NULL))
    {
        *end = frameEnd;
    }
    return st;
}

vcdStatus_t VCD_UartWriteBytes(
    vcdWriter_t *writer,
    vcdSignal_t line,
    vcdTime_t start,
    const uint8_t *data,
    size_t count,
    vcdTime_t gapTicks,
    const vcdUartConfig_t *cfg,
    vcdTime_t *end)
{
    vcdTime_t t = start;
    vcdTime_t frameEnd = start;
    size_t i;
    vcdStatus_t st = VCD_OK;

    if ((data == NULL) && (count > 0u))
    {
        return VCD_ERR_ARG;
    }
    for (i = 0u; (i < count) && (st == VCD_OK); i++)
    {
        st = VCD_UartWriteFrame(writer, line, t, data[i], cfg, 0u, &frameEnd);
        t = frameEnd + gapTicks;
    }
    if ((st == VCD_OK) && (end != NULL))
    {
        *end = frameEnd;
    }
    return st;
}

vcdStatus_t VCD_UartWriteBreak(
    vcdWriter_t *writer,
    vcdSignal_t line,
    vcdTime_t start,
    vcdTime_t duration,
    const vcdUartConfig_t *cfg)
{
    vcdStatus_t st;

    if ((cfg == NULL) || (duration == 0u) || (start >= (VCD_TIME_NONE - duration)))
    {
        return VCD_ERR_ARG;
    }
    st = VCD_ChangeBit(writer, line, start, cfg->inverted ? '1' : '0');
    if (st == VCD_OK)
    {
        st = VCD_ChangeBit(writer, line, start + duration, cfg->inverted ? '0' : '1');
    }
    return st;
}

vcdStatus_t VCD_UartDecode(
    const vcdReader_t *reader,
    size_t var,
    const vcdUartConfig_t *cfg,
    vcdUartFrameFn_t onFrame,
    void *ctx)
{
    const vcdVarInfo_t *info = VCD_ReaderVar(reader, var);
    const char *prev = NULL;
    vcdTime_t resume = 0u;
    unsigned stopPos;
    char mark;
    char space;
    size_t count;
    size_t i;

    if ((info == NULL) || (onFrame == NULL) || !configValid(cfg))
    {
        return VCD_ERR_ARG;
    }
    if (info->width != 1u)
    {
        return VCD_ERR_ARG;
    }
    mark = cfg->inverted ? '0' : '1';
    space = cfg->inverted ? '1' : '0';
    stopPos = 1u + cfg->dataBits + ((cfg->parity != VCD_UART_PARITY_NONE) ? 1u : 0u);
    count = VCD_ReaderChangeCount(reader, var);

    for (i = 0u; i < count; i++)
    {
        vcdUartFrame_t frame;
        const char *before = prev;
        const char *value;
        vcdTime_t t;
        unsigned bit;
        char level;

        (void)VCD_ReaderChange(reader, var, i, &t, &value);
        prev = value;

        /* a frame starts with a mark -> space edge once the previous frame reached its stop bit */
        if ((t < resume) || (before == NULL) || (before[0] != mark) || (value[0] != space))
        {
            continue;
        }
        if (sampleLevel(reader, var, t, cfg->bitTicks, 0u) != space)
        {
            continue; /* glitch shorter than half a bit */
        }

        memset(&frame, 0, sizeof frame);
        frame.start = t;

        for (bit = 0u; bit < cfg->dataBits; bit++)
        {
            unsigned index = cfg->msbFirst ? (cfg->dataBits - 1u - bit) : bit;
            level = sampleLevel(reader, var, t, cfg->bitTicks, 1u + bit);
            if (level == mark)
            {
                frame.data = (uint16_t)(frame.data | (1u << index));
            }
            else if (level != space)
            {
                frame.errors |= VCD_UART_ERR_UNKNOWN;
            }
        }

        if (cfg->parity != VCD_UART_PARITY_NONE)
        {
            level = sampleLevel(reader, var, t, cfg->bitTicks, stopPos - 1u);
            if ((level != mark) && (level != space))
            {
                frame.errors |= VCD_UART_ERR_UNKNOWN;
            }
            else if ((frame.errors & VCD_UART_ERR_UNKNOWN) == 0u)
            {
                unsigned got = (level == mark) ? 1u : 0u;
                if (got != parityBit(cfg->parity, frame.data))
                {
                    frame.errors |= VCD_UART_ERR_PARITY;
                }
            }
        }

        level = sampleLevel(reader, var, t, cfg->bitTicks, stopPos);
        if (level == space)
        {
            frame.errors |= VCD_UART_ERR_FRAMING;
        }
        else if (level != mark)
        {
            frame.errors |= VCD_UART_ERR_UNKNOWN;
        }

        frame.end = bitEdge(t, cfg->bitTicks, (double)stopPos + stopLength(cfg->stopBits));
        resume = bitEdge(t, cfg->bitTicks, (double)stopPos + 0.5);

        if (!onFrame(ctx, &frame))
        {
            break;
        }
    }
    return VCD_OK;
}
