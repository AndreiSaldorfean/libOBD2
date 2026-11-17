#ifndef DL_PBT_SUPPORT_H
#define DL_PBT_SUPPORT_H

/*************************************************************************************************************
 * Generators shared by the datalink property tests (positive/pbt and negative/pbt).
 *
 * An ECU response is described as wire bytes plus the virtual time each one lands on the line, relative
 * to the end of the tester's request (when P2 starts). DLFX_PBT_QueueWire() puts it on the dl_fixture
 * line, rewinds the clock and starts P2, exactly like a request that just ended.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "dl_fixture.h"
#include "theft.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ================================================= MACROS ================================================ */
#define DLFX_PBT_WIRE_MAX     20u   /* bytes of one generated response */
#define DLFX_PBT_ECU_FMT      0x48u /* ISO 9141-2 response header: format ... */
#define DLFX_PBT_ECU_TRG      0x6Bu /* ... and target (the tester); the source is the ECU address */
#define DLFX_PBT_HEADER_LEN   3u
#define DLFX_PBT_MAX_DATA     7u    /* ISO 9141-2: at most 7 data bytes (sid + 6) */
#define DLFX_PBT_SPAN_CAP     100u  /* generated timing stays within the first 100 ms of a window */

/* Upper end of a generated time inside a window [lo, hi]: one poll short of hi, and never more than
 * DLFX_PBT_SPAN_CAP past lo, so trials stay short when a window is 5 s wide */
#define DLFX_PBT_IN_WINDOW_MAX(lo, hi) \
    ((((hi) - (lo)) > DLFX_PBT_SPAN_CAP) ? ((lo) + DLFX_PBT_SPAN_CAP) : ((hi) - 2u))

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ECU side of a 5-baud init: the four bytes and W1..W4 (W1 from the end of the 5-baud address, W2/W3
 * from the previous ECU byte, W4 from the tester's ~KB2) */
typedef struct
{
    uint8_t  sync;
    uint8_t  kb1;
    uint8_t  kb2;
    uint8_t  invAddr;
    uint32_t w1;
    uint32_t w2;
    uint32_t w3;
    uint32_t w4;
} dlFxInit_t;

typedef struct
{
    uint8_t  len;
    uint8_t  byte[DLFX_PBT_WIRE_MAX];
    uint32_t at[DLFX_PBT_WIRE_MAX]; /* ms after the end of the request, non-decreasing */
} dlFxWire_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

/* Generates a valid ECU response: header 48 6B <src>, 'dataLen' data bytes (sid first, never 7F unless
 * 'sid' says so: pass -1 for a random sid), checksum, first byte inside P2, the rest inside P1. */
void DLFX_PBT_GenResponse(struct theft *t, dlFxWire_t *w, uint8_t dataLen, int sid);

/* ISO9141 checksum (byte sum) of the first 'len' bytes */
uint8_t DLFX_PBT_Checksum(const uint8_t *bytes, size_t len);

/* True when a byte after the header, before the last one, equals the sum of the bytes before it. A
 * checksum-framed receiver would end the frame there. */
bool DLFX_PBT_HasEarlyChecksum(const dlFxWire_t *w);

/* Sets up the dl_fixture line with a 1 ms poll step, queues 'w' and starts P2 at t=0 */
void DLFX_PBT_QueueWire(const dlFxWire_t *w);

void DLFX_PBT_PrintWire(FILE *f, const dlFxWire_t *w);

/* Sets up the dl_fixture line with a 1 ms poll step and queues the ECU side of a 5-baud init. The
 * virtual line echoes the bit-banged address, so every ECU byte must be queued before the tester runs:
 * the inverted address is placed W4 after the time the tester sends ~KB2, taken from a model of a
 * tester that polls every 1 ms. Returns that send time; check it with DLFX_PBT_SentAt() afterwards. */
uint32_t DLFX_PBT_QueueInit(const dlFxInit_t *c);

/* True when the tester put 'byte' on the line at 'time' (searches what is left on the line) */
bool DLFX_PBT_SentAt(uint8_t byte, uint32_t time);

void DLFX_PBT_PrintInit(FILE *f, const dlFxInit_t *c);

#endif /* DL_PBT_SUPPORT_H */
