#ifndef VCD_TYPES_H
#define VCD_TYPES_H

/*************************************************************************************************************
 * VCDio - common types shared by the writer, reader and protocol helpers.
 *
 * Time is always an integer count of timescale ticks (vcdTime_t). A timescale is a magnitude (1, 10 or 100)
 * and a unit, e.g. {1, VCD_UNIT_US} means one tick is one microsecond.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================= MACROS ================================================ */
#define VCDIO_VERSION_MAJOR  0
#define VCDIO_VERSION_MINOR  1
#define VCDIO_VERSION_PATCH  0
#define VCDIO_VERSION_STRING "0.1.0"

/* "No time". Never a valid change time; used e.g. to close a writer without an end marker. */
#define VCD_TIME_NONE UINT64_MAX

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef uint64_t vcdTime_t;

typedef enum
{
    VCD_OK = 0,
    VCD_ERR_ARG,       /* invalid argument: NULL, bad name/width, wrong signal kind for the call */
    VCD_ERR_NOMEM,     /* allocation failed */
    VCD_ERR_IO,        /* file could not be opened/read/written, or the sink callback failed */
    VCD_ERR_STATE,     /* call not allowed now, e.g. declaring a signal after data was recorded */
    VCD_ERR_TIME,      /* time is earlier than what was already committed */
    VCD_ERR_VALUE,     /* value does not fit the signal or contains invalid characters */
    VCD_ERR_PARSE,     /* malformed VCD input */
    VCD_ERR_NOT_FOUND, /* lookup failed */
} vcdStatus_t;

typedef enum
{
    VCD_UNIT_S = 0,
    VCD_UNIT_MS,
    VCD_UNIT_US,
    VCD_UNIT_NS,
    VCD_UNIT_PS,
    VCD_UNIT_FS,
} vcdUnit_t;

typedef struct
{
    uint16_t  magnitude; /* 1, 10 or 100 */
    vcdUnit_t unit;
} vcdTimescale_t;

typedef enum
{
    VCD_VAR_WIRE = 0,  /* digital, 1..N bits */
    VCD_VAR_REG,       /* digital, 1..N bits */
    VCD_VAR_INTEGER,   /* digital, usually 32 bits */
    VCD_VAR_REAL,      /* double precision value */
    VCD_VAR_STRING,    /* text (GTKWave/Surfer extension), whitespace is not allowed in values */
    VCD_VAR_EVENT,     /* reader only */
    VCD_VAR_PARAMETER, /* reader only */
    VCD_VAR_OTHER,     /* reader only: any other type keyword (tri, supply0, ...) */
} vcdVarType_t;

/* =============================================== MODULE API ============================================== */

/* Human readable text for a status code. Never returns NULL. */
const char *VCD_StatusStr(vcdStatus_t status);

/* "s", "ms", "us", "ns", "ps", "fs" ("?" for invalid values). */
const char *VCD_UnitStr(vcdUnit_t unit);

/* Keyword used in $var for this type ("wire", "reg", ...). */
const char *VCD_VarTypeStr(vcdVarType_t type);

/* True when magnitude is 1, 10 or 100 and the unit is valid. */
bool VCD_TimescaleValid(vcdTimescale_t ts);

/* Length of one tick in seconds (0 for an invalid unit). */
double VCD_TimescaleSeconds(vcdTimescale_t ts);

double VCD_TicksToSeconds(vcdTimescale_t ts, vcdTime_t ticks);

/* Rounds to the nearest tick. Negative or NaN input gives 0. */
vcdTime_t VCD_SecondsToTicks(vcdTimescale_t ts, double seconds);

/* Converts a binary value as stored by the reader ("1010", optionally "b1010") to an integer.
 * VCD_ERR_VALUE if it holds x/z/other characters or needs more than 64 bits. */
vcdStatus_t VCD_ValueToU64(const char *value, uint64_t *out);

#ifdef __cplusplus
}
#endif

#endif /* VCD_TYPES_H */
