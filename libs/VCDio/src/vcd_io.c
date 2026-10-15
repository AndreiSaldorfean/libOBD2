/* ================================================ INCLUDES =============================================== */
#include "vcd_io.h"
#include "vcd_internal.h"
#include <math.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define ID_FIRST_CHAR '!'
#define ID_RADIX      94u /* printable ASCII '!'..'~' */

/* ============================================ LOCAL VARIABLES ============================================ */
static const double unitSeconds[] = {1.0, 1e-3, 1e-6, 1e-9, 1e-12, 1e-15};
static const char *const unitNames[] = {"s", "ms", "us", "ns", "ps", "fs"};

/* ================================================ MODULE API ============================================= */
const char *VCD_StatusStr(vcdStatus_t status)
{
    switch (status)
    {
        case VCD_OK:
            return "ok";
        case VCD_ERR_ARG:
            return "invalid argument";
        case VCD_ERR_NOMEM:
            return "out of memory";
        case VCD_ERR_IO:
            return "I/O error";
        case VCD_ERR_STATE:
            return "not allowed in the current state";
        case VCD_ERR_TIME:
            return "time is earlier than already committed time";
        case VCD_ERR_VALUE:
            return "value does not fit the signal";
        case VCD_ERR_PARSE:
            return "malformed VCD input";
        case VCD_ERR_NOT_FOUND:
            return "not found";
        default:
            return "unknown status";
    }
}

const char *VCD_UnitStr(vcdUnit_t unit)
{
    if ((unsigned)unit > (unsigned)VCD_UNIT_FS)
    {
        return "?";
    }
    return unitNames[unit];
}

const char *VCD_VarTypeStr(vcdVarType_t type)
{
    switch (type)
    {
        case VCD_VAR_WIRE:
            return "wire";
        case VCD_VAR_REG:
            return "reg";
        case VCD_VAR_INTEGER:
            return "integer";
        case VCD_VAR_REAL:
            return "real";
        case VCD_VAR_STRING:
            return "string";
        case VCD_VAR_EVENT:
            return "event";
        case VCD_VAR_PARAMETER:
            return "parameter";
        default:
            return "other";
    }
}

bool VCD_TimescaleValid(vcdTimescale_t ts)
{
    bool magnitudeOk = (ts.magnitude == 1u) || (ts.magnitude == 10u) || (ts.magnitude == 100u);
    return magnitudeOk && ((unsigned)ts.unit <= (unsigned)VCD_UNIT_FS);
}

double VCD_TimescaleSeconds(vcdTimescale_t ts)
{
    if ((unsigned)ts.unit > (unsigned)VCD_UNIT_FS)
    {
        return 0.0;
    }
    return (double)ts.magnitude * unitSeconds[ts.unit];
}

double VCD_TicksToSeconds(vcdTimescale_t ts, vcdTime_t ticks)
{
    return (double)ticks * VCD_TimescaleSeconds(ts);
}

vcdTime_t VCD_SecondsToTicks(vcdTimescale_t ts, double seconds)
{
    double tick = VCD_TimescaleSeconds(ts);
    double ticks;

    if (!(seconds > 0.0) || !(tick > 0.0))
    {
        return 0u;
    }

    ticks = floor((seconds / tick) + 0.5);
    if (ticks >= 18446744073709549568.0) /* largest double below 2^64 */
    {
        return VCD_TIME_NONE - 1u;
    }
    return (vcdTime_t)ticks;
}

vcdStatus_t VCD_ValueToU64(const char *value, uint64_t *out)
{
    const char *p = value;
    uint64_t v = 0u;

    if ((value == NULL) || (out == NULL))
    {
        return VCD_ERR_ARG;
    }
    if ((*p == 'b') || (*p == 'B'))
    {
        p++;
    }
    if (*p == '\0')
    {
        return VCD_ERR_VALUE;
    }

    for (; *p != '\0'; p++)
    {
        if ((*p != '0') && (*p != '1'))
        {
            return VCD_ERR_VALUE;
        }
        if ((v >> 63) != 0u)
        {
            return VCD_ERR_VALUE;
        }
        v = (v << 1) | (uint64_t)(*p - '0');
    }

    *out = v;
    return VCD_OK;
}

/* ============================================ INTERNAL HELPERS =========================================== */
size_t vcdEncodeId(uint32_t index, char *buf)
{
    size_t len = 0u;
    uint32_t n = index;

    for (;;)
    {
        buf[len++] = (char)(ID_FIRST_CHAR + (char)(n % ID_RADIX));
        n /= ID_RADIX;
        if (n == 0u)
        {
            break;
        }
        n--;
    }
    buf[len] = '\0';
    return len;
}

void *vcdGrow(void *array, size_t *cap, size_t need, size_t elemSize)
{
    size_t newCap;
    void *p;

    if (need <= *cap)
    {
        return array;
    }

    newCap = (*cap != 0u) ? *cap : 16u;
    while (newCap < need)
    {
        if (newCap > (SIZE_MAX / 2u))
        {
            return NULL;
        }
        newCap *= 2u;
    }
    if (newCap > (SIZE_MAX / elemSize))
    {
        return NULL;
    }

    p = VCD_REALLOC(array, newCap * elemSize);
    if (p != NULL)
    {
        *cap = newCap;
    }
    return p;
}

char *vcdStrNDup(const char *s, size_t n)
{
    char *d = (char *)VCD_MALLOC(n + 1u);
    if (d != NULL)
    {
        memcpy(d, s, n);
        d[n] = '\0';
    }
    return d;
}

char *vcdStrDup(const char *s)
{
    return vcdStrNDup(s, strlen(s));
}
