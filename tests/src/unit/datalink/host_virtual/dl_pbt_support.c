/* ================================================ INCLUDES =============================================== */
#include "dl_pbt_support.h"
#include "datalink_utils.h"
#include "pbt_support.h"
#include <inttypes.h>

/* ================================================= MACROS ================================================ */
#define NEGATIVE_RESPONSE_SID 0x7Fu
#define INIT_BITS             10u                                                 /* start + 8 + stop */
#define INIT_ADDR_END         (ISO9141_W5_TIME_MIN + INIT_BITS * (1000u / 5u))  /* 5-baud address sent */

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void DLFX_PBT_GenResponse(struct theft *t, dlFxWire_t *w, uint8_t dataLen, int sid)
{
    uint32_t time = 0u;
    uint8_t idx = 0u;

    w->len = 0u;
    w->byte[w->len++] = DLFX_PBT_ECU_FMT;
    w->byte[w->len++] = DLFX_PBT_ECU_TRG;
    w->byte[w->len++] = PBT_Byte(t);
    for (idx = 0u; idx < dataLen; idx++)
    {
        w->byte[w->len++] = PBT_Byte(t);
    }
    if (sid >= 0)
    {
        w->byte[DLFX_PBT_HEADER_LEN] = (uint8_t)sid;
    }
    else if (w->byte[DLFX_PBT_HEADER_LEN] == NEGATIVE_RESPONSE_SID)
    {
        w->byte[DLFX_PBT_HEADER_LEN] = 0x41u;
    }
    w->byte[w->len] = DLFX_PBT_Checksum(w->byte, w->len);
    w->len++;

    time = PBT_Range(t, P2_TIME_MIN, DLFX_PBT_IN_WINDOW_MAX(P2_TIME_MIN, P2_TIME_MAX));
    for (idx = 0u; idx < w->len; idx++)
    {
        if (idx > 0u)
        {
            time += PBT_Range(t, P1_TIME_MIN, DLFX_PBT_IN_WINDOW_MAX(P1_TIME_MIN, P1_TIME_MAX) - 1u);
        }
        w->at[idx] = time;
    }
}

uint8_t DLFX_PBT_Checksum(const uint8_t *bytes, size_t len)
{
    uint8_t sum = 0u;

    for (size_t idx = 0u; idx < len; idx++)
    {
        sum = (uint8_t)(sum + bytes[idx]);
    }
    return sum;
}

bool DLFX_PBT_HasEarlyChecksum(const dlFxWire_t *w)
{
    for (uint8_t idx = DLFX_PBT_HEADER_LEN; (idx + 1u) < w->len; idx++)
    {
        if (w->byte[idx] == DLFX_PBT_Checksum(w->byte, idx))
        {
            return true;
        }
    }
    return false;
}

void DLFX_PBT_QueueWire(const dlFxWire_t *w)
{
    DLFX_SetUpLine();
    dlFx.timer.timeStep = 1u;
    for (uint8_t idx = 0u; idx < w->len; idx++)
    {
        DLFX_LineWriteAt(w->at[idx], w->byte[idx]);
    }
    dlFx.timer.timeNow = 0u;
    (void)DL_StartTimeout(&dlFx.link, P2_TIME_MAX);
}

void DLFX_PBT_PrintWire(FILE *f, const dlFxWire_t *w)
{
    for (uint8_t idx = 0u; idx < w->len; idx++)
    {
        fprintf(f, "%s%02X@%" PRIu32, (idx != 0u) ? " " : "", w->byte[idx], w->at[idx]);
    }
}

uint32_t DLFX_PBT_QueueInit(const dlFxInit_t *c)
{
    const uint32_t syncAt = INIT_ADDR_END + c->w1;
    const uint32_t kb1At  = syncAt + c->w2;
    const uint32_t kb2At  = kb1At + c->w3;
    uint32_t now = 0u;
    uint32_t sendAt = 0u;

    /* Tester model: polls every 1 ms from the end of the address on, a read costs 1 ms, a byte is
     * visible from 1 ms after it landed; then it waits W4 min and sends ~KB2 */
    now = syncAt + 2u;
    now = ((kb1At + 1u > now) ? (kb1At + 1u) : now) + 1u;
    now = ((kb2At + 1u > now) ? (kb2At + 1u) : now) + 1u;
    sendAt = now + ISO9141_W4_TIME_MIN;

    DLFX_SetUpLine();
    dlFx.timer.timeStep = 1u;
    DLFX_LineWriteAt(syncAt, c->sync);
    DLFX_LineWriteAt(kb1At, c->kb1);
    DLFX_LineWriteAt(kb2At, c->kb2);
    DLFX_LineWriteAt(sendAt + c->w4, c->invAddr);
    dlFx.timer.timeNow = 0u;
    return sendAt;
}

bool DLFX_PBT_SentAt(uint8_t byte, uint32_t time)
{
    uartEvent_t ev = {0};

    while (q_pop(&dlFx.uart.eventQueue, &ev))
    {
        if ((ev.byte == byte) && (ev.timeReceived == time))
        {
            return true;
        }
    }
    return false;
}

void DLFX_PBT_PrintInit(FILE *f, const dlFxInit_t *c)
{
    fprintf(f, "ECU %02X %02X %02X %02X, W1=%" PRIu32 " W2=%" PRIu32 " W3=%" PRIu32 " W4=%" PRIu32 " ms",
            c->sync, c->kb1, c->kb2, c->invAddr, c->w1, c->w2, c->w3, c->w4);
}
