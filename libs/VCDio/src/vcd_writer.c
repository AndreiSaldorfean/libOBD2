/* ================================================ INCLUDES =============================================== */
#include "vcd_writer.h"
#include "vcd_internal.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ================================================= MACROS ================================================ */
#define OUT_BUF_SIZE    4096u
#define MAX_VAR_WIDTH   65535u
#define DEFAULT_INT_WID 32u
#define STACK_TOKEN_LEN 128u /* tokens up to this length are built on the stack */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    vcdVarType_t type;
    uint32_t     width;
    char         id[VCD_ID_MAX_LEN];
    char        *last;      /* last written value token, NULL until the first write */
    size_t       lastCap;
    size_t       groupMark; /* 1 + index of this signal's final change in the group being written, else 0 */
} sigInfo_t;

/* A recorded change waiting to be written. Its value token ("1", "b1010", "r2.5", "sTEXT") lives in the
 * pool at valOff. seq keeps changes at the same time in call order. */
typedef struct
{
    vcdTime_t time;
    uint64_t  seq;
    uint32_t  sig;
    uint32_t  valOff;
} pending_t;

struct vcdWriter
{
    vcdOrder_t     order;
    bool           dropRedundant;
    vcdTimescale_t timescale;

    vcdSinkFn_t sink;
    void       *sinkCtx;
    FILE       *file; /* owned when opened with VCD_WriterOpenFile() */

    char        out[OUT_BUF_SIZE];
    size_t      outLen;
    vcdStatus_t err; /* sticky VCD_ERR_IO / VCD_ERR_NOMEM */

    sigInfo_t *sigs;
    size_t     sigCount;
    size_t     sigCap;
    uint32_t   scopeDepth;
    bool       defsDone;
    bool       dumpDone;

    pending_t *pend;
    size_t     pendCount;
    size_t     pendCap;
    bool       pendSorted;
    char      *pool;
    size_t     poolLen;
    size_t     poolCap;
    uint64_t   seq;
    vcdTime_t  watermark; /* changes before this time are final and rejected */

    bool      stamped; /* at least one '#time' written */
    vcdTime_t lastStamp;
};

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static bool fileSink(void *ctx, const char *data, size_t len);
static void outFlush(vcdWriter_t *w);
static void outWrite(vcdWriter_t *w, const char *data, size_t len);
static void outStr(vcdWriter_t *w, const char *s);
static void outTime(vcdWriter_t *w, vcdTime_t t);
static void writeSection(vcdWriter_t *w, const char *keyword, const char *text);
static void writeDate(vcdWriter_t *w);
static bool validName(const char *name);
static bool validText(const char *text);
static bool isDigital(vcdVarType_t type);
static bool isScalar(const sigInfo_t *s);
static char normBit(char c);
static void finishDefinitions(vcdWriter_t *w);
static void setLast(vcdWriter_t *w, sigInfo_t *s, const char *tok);
static void writeValue(vcdWriter_t *w, sigInfo_t *s, const char *tok);
static int comparePending(const void *a, const void *b);
static void emitGroup(vcdWriter_t *w, size_t from, size_t to);
static void dropPending(vcdWriter_t *w, size_t n);
static void emitBefore(vcdWriter_t *w, vcdTime_t limit);
static vcdStatus_t checkWriter(const vcdWriter_t *w);
static vcdStatus_t lookupSignal(vcdWriter_t *w, vcdSignal_t signal, sigInfo_t **out);
static vcdStatus_t addChange(vcdWriter_t *w, vcdSignal_t signal, vcdTime_t t, const char *tok, size_t len);
static vcdStatus_t writerCreate(
    vcdWriter_t **writer,
    vcdSinkFn_t sink,
    void *ctx,
    FILE *file,
    const vcdWriterConfig_t *cfg);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static bool fileSink(void *ctx, const char *data, size_t len)
{
    return fwrite(data, 1u, len, (FILE *)ctx) == len;
}

static void outFlush(vcdWriter_t *w)
{
    if ((w->outLen > 0u) && (w->err == VCD_OK) && !w->sink(w->sinkCtx, w->out, w->outLen))
    {
        w->err = VCD_ERR_IO;
    }
    w->outLen = 0u;
}

static void outWrite(vcdWriter_t *w, const char *data, size_t len)
{
    if (w->err != VCD_OK)
    {
        return;
    }
    if (len > (OUT_BUF_SIZE - w->outLen))
    {
        outFlush(w);
        if (len >= OUT_BUF_SIZE)
        {
            if ((w->err == VCD_OK) && !w->sink(w->sinkCtx, data, len))
            {
                w->err = VCD_ERR_IO;
            }
            return;
        }
    }
    memcpy(w->out + w->outLen, data, len);
    w->outLen += len;
}

static void outStr(vcdWriter_t *w, const char *s)
{
    outWrite(w, s, strlen(s));
}

static void outTime(vcdWriter_t *w, vcdTime_t t)
{
    char buf[32];
    int n = snprintf(buf, sizeof buf, "#%" PRIu64 "\n", t);
    outWrite(w, buf, (size_t)n);
}

static void writeSection(vcdWriter_t *w, const char *keyword, const char *text)
{
    outStr(w, keyword);
    outStr(w, "\n\t");
    outStr(w, text);
    outStr(w, "\n$end\n");
}

static void writeDate(vcdWriter_t *w)
{
#ifndef VCDIO_NO_TIME
    char buf[64];
    time_t now = time(NULL);
    struct tm *local = localtime(&now);

    if ((local != NULL) && (strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", local) > 0u))
    {
        writeSection(w, "$date", buf);
    }
#else
    (void)w;
#endif
}

/* Identifiers and scope names: printable, no whitespace, not starting with '$'. */
static bool validName(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;

    if ((name == NULL) || (name[0] == '\0') || (name[0] == '$'))
    {
        return false;
    }
    for (; *p != 0u; p++)
    {
        if ((*p <= (unsigned char)' ') || (*p >= 0x7Fu))
        {
            return false;
        }
    }
    return true;
}

/* Header text may not contain "$end", which would terminate its section early. */
static bool validText(const char *text)
{
    return (text == NULL) || (strstr(text, "$end") == NULL);
}

static bool isDigital(vcdVarType_t type)
{
    return (type == VCD_VAR_WIRE) || (type == VCD_VAR_REG) || (type == VCD_VAR_INTEGER);
}

static bool isScalar(const sigInfo_t *s)
{
    return isDigital(s->type) && (s->width == 1u);
}

static char normBit(char c)
{
    switch (c)
    {
        case '0':
        case '1':
        case 'x':
        case 'z':
            return c;
        case 'X':
            return 'x';
        case 'Z':
            return 'z';
        default:
            return '\0';
    }
}

static void finishDefinitions(vcdWriter_t *w)
{
    if (w->defsDone)
    {
        return;
    }
    while (w->scopeDepth > 0u)
    {
        outStr(w, "$upscope $end\n");
        w->scopeDepth--;
    }
    outStr(w, "$enddefinitions $end\n");
    w->defsDone = true;
}

static void setLast(vcdWriter_t *w, sigInfo_t *s, const char *tok)
{
    size_t len = strlen(tok) + 1u;
    char *p = (char *)vcdGrow(s->last, &s->lastCap, len, 1u);

    if (p == NULL)
    {
        w->err = VCD_ERR_NOMEM;
        return;
    }
    s->last = p;
    memcpy(s->last, tok, len);
}

static void writeValue(vcdWriter_t *w, sigInfo_t *s, const char *tok)
{
    outStr(w, tok);
    if (!isScalar(s))
    {
        outWrite(w, " ", 1u);
    }
    outStr(w, s->id);
    outWrite(w, "\n", 1u);
    setLast(w, s, tok);
}

static int comparePending(const void *a, const void *b)
{
    const pending_t *pa = (const pending_t *)a;
    const pending_t *pb = (const pending_t *)b;

    if (pa->time != pb->time)
    {
        return (pa->time < pb->time) ? -1 : 1;
    }
    if (pa->seq != pb->seq)
    {
        return (pa->seq < pb->seq) ? -1 : 1;
    }
    return 0;
}

/* Writes pend[from..to), which all share one time. */
static void emitGroup(vcdWriter_t *w, size_t from, size_t to)
{
    const vcdTime_t t = w->pend[from].time;
    size_t i;

    /* Mark each signal's final change in the group; earlier ones at the same time are dropped. */
    for (i = from; i < to; i++)
    {
        w->sigs[w->pend[i].sig].groupMark = i + 1u;
    }

    if (!w->dumpDone)
    {
        outTime(w, t);
        outStr(w, "$dumpvars\n");
        for (i = 0u; i < w->sigCount; i++)
        {
            sigInfo_t *s = &w->sigs[i];
            const char *tok = NULL;

            if (s->groupMark != 0u)
            {
                tok = w->pool + w->pend[s->groupMark - 1u].valOff;
            }
            else if (isDigital(s->type))
            {
                tok = isScalar(s) ? "x" : "bx";
            }

            if (tok != NULL)
            {
                writeValue(w, s, tok);
            }
        }
        outStr(w, "$end\n");
        w->dumpDone = true;
        w->stamped = true;
        w->lastStamp = t;
    }
    else
    {
        bool stamped = false;

        for (i = from; i < to; i++)
        {
            const pending_t *p = &w->pend[i];
            sigInfo_t *s = &w->sigs[p->sig];
            const char *tok = w->pool + p->valOff;

            if (s->groupMark != (i + 1u))
            {
                continue;
            }
            if (w->dropRedundant && (s->last != NULL) && (strcmp(s->last, tok) == 0))
            {
                continue;
            }
            if (!stamped)
            {
                outTime(w, t);
                stamped = true;
                w->stamped = true;
                w->lastStamp = t;
            }
            writeValue(w, s, tok);
        }
    }

    for (i = from; i < to; i++)
    {
        w->sigs[w->pend[i].sig].groupMark = 0u;
    }
}

/* Forgets the first n (already written) pending changes and compacts the value pool. */
static void dropPending(vcdWriter_t *w, size_t n)
{
    size_t keep = w->pendCount - n;
    size_t i;
    size_t need = 0u;
    char *pool;

    if (n == 0u)
    {
        return;
    }
    if (keep == 0u)
    {
        w->pendCount = 0u;
        w->poolLen = 0u;
        return;
    }

    memmove(w->pend, w->pend + n, keep * sizeof *w->pend);
    w->pendCount = keep;

    for (i = 0u; i < keep; i++)
    {
        need += strlen(w->pool + w->pend[i].valOff) + 1u;
    }
    pool = (char *)VCD_MALLOC(need);
    if (pool == NULL)
    {
        return; /* keep the old pool, offsets are still valid */
    }

    need = 0u;
    for (i = 0u; i < keep; i++)
    {
        size_t len = strlen(w->pool + w->pend[i].valOff) + 1u;
        memcpy(pool + need, w->pool + w->pend[i].valOff, len);
        w->pend[i].valOff = (uint32_t)need;
        need += len;
    }
    VCD_FREE(w->pool);
    w->pool = pool;
    w->poolLen = need;
    w->poolCap = need;
}

/* Writes all pending changes earlier than 'limit'. */
static void emitBefore(vcdWriter_t *w, vcdTime_t limit)
{
    size_t n = 0u;
    size_t i;
    size_t j;

    finishDefinitions(w);
    if (w->pendCount == 0u)
    {
        return;
    }
    if (!w->pendSorted)
    {
        qsort(w->pend, w->pendCount, sizeof *w->pend, comparePending);
        w->pendSorted = true;
    }

    while ((n < w->pendCount) && (w->pend[n].time < limit))
    {
        n++;
    }
    for (i = 0u; i < n; i = j)
    {
        for (j = i + 1u; (j < n) && (w->pend[j].time == w->pend[i].time); j++)
        {
        }
        emitGroup(w, i, j);
    }
    dropPending(w, n);
}

static vcdStatus_t checkWriter(const vcdWriter_t *w)
{
    if (w == NULL)
    {
        return VCD_ERR_ARG;
    }
    return w->err;
}

static vcdStatus_t lookupSignal(vcdWriter_t *w, vcdSignal_t signal, sigInfo_t **out)
{
    vcdStatus_t st = checkWriter(w);

    if (st != VCD_OK)
    {
        return st;
    }
    if ((signal < 0) || ((size_t)signal >= w->sigCount))
    {
        return VCD_ERR_ARG;
    }
    *out = &w->sigs[signal];
    return VCD_OK;
}

static vcdStatus_t addChange(vcdWriter_t *w, vcdSignal_t signal, vcdTime_t t, const char *tok, size_t len)
{
    pending_t *p;
    char *pool;

    if (t == VCD_TIME_NONE)
    {
        return VCD_ERR_ARG;
    }
    if (t < w->watermark)
    {
        return VCD_ERR_TIME;
    }

    finishDefinitions(w);
    if ((w->order == VCD_ORDER_STRICT) && (t > w->watermark))
    {
        emitBefore(w, t);
        w->watermark = t;
    }
    if (w->err != VCD_OK)
    {
        return w->err;
    }

    if ((len + 1u) > ((size_t)UINT32_MAX - w->poolLen))
    {
        return VCD_ERR_NOMEM;
    }
    pool = (char *)vcdGrow(w->pool, &w->poolCap, w->poolLen + len + 1u, 1u);
    if (pool == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    w->pool = pool;
    p = (pending_t *)vcdGrow(w->pend, &w->pendCap, w->pendCount + 1u, sizeof *w->pend);
    if (p == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    w->pend = p;

    if ((w->pendCount > 0u) && (t < w->pend[w->pendCount - 1u].time))
    {
        w->pendSorted = false;
    }

    memcpy(w->pool + w->poolLen, tok, len);
    w->pool[w->poolLen + len] = '\0';

    p = &w->pend[w->pendCount++];
    p->time = t;
    p->seq = w->seq++;
    p->sig = (uint32_t)signal;
    p->valOff = (uint32_t)w->poolLen;
    w->poolLen += len + 1u;
    return VCD_OK;
}

static vcdStatus_t writerCreate(
    vcdWriter_t **writer,
    vcdSinkFn_t sink,
    void *ctx,
    FILE *file,
    const vcdWriterConfig_t *cfg)
{
    vcdWriterConfig_t def;
    vcdWriter_t *w;
    char ts[16];

    if (cfg == NULL)
    {
        VCD_WriterConfigDefault(&def);
        cfg = &def;
    }

    w = (vcdWriter_t *)VCD_MALLOC(sizeof *w);
    if (w == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    memset(w, 0, sizeof *w);
    w->order = cfg->order;
    w->dropRedundant = cfg->dropRedundant;
    w->timescale = cfg->timescale;
    w->sink = sink;
    w->sinkCtx = ctx;
    w->file = file;
    w->pendSorted = true;

    if (cfg->date == NULL)
    {
        writeDate(w);
    }
    else if (cfg->date[0] != '\0')
    {
        writeSection(w, "$date", cfg->date);
    }

    if (cfg->version == NULL)
    {
        writeSection(w, "$version", "VCDio " VCDIO_VERSION_STRING);
    }
    else if (cfg->version[0] != '\0')
    {
        writeSection(w, "$version", cfg->version);
    }

    if ((cfg->comment != NULL) && (cfg->comment[0] != '\0'))
    {
        writeSection(w, "$comment", cfg->comment);
    }

    (void)snprintf(ts, sizeof ts, "%u%s", (unsigned)cfg->timescale.magnitude, VCD_UnitStr(cfg->timescale.unit));
    outStr(w, "$timescale ");
    outStr(w, ts);
    outStr(w, " $end\n");

    *writer = w;
    return VCD_OK;
}

static vcdStatus_t validateConfig(const vcdWriterConfig_t *cfg)
{
    if (cfg == NULL)
    {
        return VCD_OK;
    }
    if (!VCD_TimescaleValid(cfg->timescale))
    {
        return VCD_ERR_ARG;
    }
    if ((cfg->order != VCD_ORDER_STRICT) && (cfg->order != VCD_ORDER_SORTED))
    {
        return VCD_ERR_ARG;
    }
    if (!validText(cfg->date) || !validText(cfg->version) || !validText(cfg->comment))
    {
        return VCD_ERR_ARG;
    }
    return VCD_OK;
}

/* ================================================ MODULE API ============================================= */
void VCD_WriterConfigDefault(vcdWriterConfig_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }
    memset(cfg, 0, sizeof *cfg);
    cfg->timescale.magnitude = 1u;
    cfg->timescale.unit = VCD_UNIT_NS;
    cfg->order = VCD_ORDER_STRICT;
    cfg->dropRedundant = true;
}

vcdStatus_t VCD_WriterOpenFile(vcdWriter_t **writer, const char *path, const vcdWriterConfig_t *cfg)
{
    FILE *f;
    vcdStatus_t st;

    if ((writer == NULL) || (path == NULL))
    {
        return VCD_ERR_ARG;
    }
    *writer = NULL;
    st = validateConfig(cfg);
    if (st != VCD_OK)
    {
        return st;
    }

    f = fopen(path, "wb");
    if (f == NULL)
    {
        return VCD_ERR_IO;
    }
    st = writerCreate(writer, fileSink, f, f, cfg);
    if (st != VCD_OK)
    {
        (void)fclose(f);
    }
    return st;
}

vcdStatus_t VCD_WriterOpenSink(vcdWriter_t **writer, vcdSinkFn_t sink, void *ctx, const vcdWriterConfig_t *cfg)
{
    vcdStatus_t st;

    if ((writer == NULL) || (sink == NULL))
    {
        return VCD_ERR_ARG;
    }
    *writer = NULL;
    st = validateConfig(cfg);
    if (st != VCD_OK)
    {
        return st;
    }
    return writerCreate(writer, sink, ctx, NULL, cfg);
}

vcdStatus_t VCD_WriterClose(vcdWriter_t *writer, vcdTime_t endTime)
{
    vcdStatus_t st = VCD_OK;
    size_t i;

    if (writer == NULL)
    {
        return VCD_ERR_ARG;
    }

    emitBefore(writer, VCD_TIME_NONE);
    if (endTime != VCD_TIME_NONE)
    {
        if (!writer->stamped || (endTime > writer->lastStamp))
        {
            outTime(writer, endTime);
        }
        else if (endTime < writer->lastStamp)
        {
            st = VCD_ERR_TIME;
        }
    }
    outFlush(writer);

    if ((writer->file != NULL) && (fclose(writer->file) != 0) && (writer->err == VCD_OK))
    {
        writer->err = VCD_ERR_IO;
    }
    if (writer->err != VCD_OK)
    {
        st = writer->err;
    }

    for (i = 0u; i < writer->sigCount; i++)
    {
        VCD_FREE(writer->sigs[i].last);
    }
    VCD_FREE(writer->sigs);
    VCD_FREE(writer->pend);
    VCD_FREE(writer->pool);
    VCD_FREE(writer);
    return st;
}

vcdStatus_t VCD_ScopeBegin(vcdWriter_t *writer, const char *name)
{
    vcdStatus_t st = checkWriter(writer);

    if (st != VCD_OK)
    {
        return st;
    }
    if (writer->defsDone)
    {
        return VCD_ERR_STATE;
    }
    if (!validName(name))
    {
        return VCD_ERR_ARG;
    }

    outStr(writer, "$scope module ");
    outStr(writer, name);
    outStr(writer, " $end\n");
    writer->scopeDepth++;
    return writer->err;
}

vcdStatus_t VCD_ScopeEnd(vcdWriter_t *writer)
{
    vcdStatus_t st = checkWriter(writer);

    if (st != VCD_OK)
    {
        return st;
    }
    if (writer->defsDone || (writer->scopeDepth == 0u))
    {
        return VCD_ERR_STATE;
    }

    outStr(writer, "$upscope $end\n");
    writer->scopeDepth--;
    return writer->err;
}

vcdStatus_t VCD_AddVar(
    vcdWriter_t *writer,
    vcdVarType_t type,
    uint32_t width,
    const char *name,
    vcdSignal_t *signal)
{
    vcdStatus_t st = checkWriter(writer);
    sigInfo_t *sigs;
    sigInfo_t *s;
    char line[48];

    if (st != VCD_OK)
    {
        return st;
    }
    if (writer->defsDone)
    {
        return VCD_ERR_STATE;
    }
    if (!validName(name) || (writer->sigCount >= (size_t)INT32_MAX))
    {
        return VCD_ERR_ARG;
    }

    switch (type)
    {
        case VCD_VAR_INTEGER:
            width = (width == 0u) ? DEFAULT_INT_WID : width;
            /* fall through */
        case VCD_VAR_WIRE:
        case VCD_VAR_REG:
            if ((width == 0u) || (width > MAX_VAR_WIDTH))
            {
                return VCD_ERR_ARG;
            }
            break;
        case VCD_VAR_REAL:
            width = 64u;
            break;
        case VCD_VAR_STRING:
            width = 1u;
            break;
        default:
            return VCD_ERR_ARG;
    }

    sigs = (sigInfo_t *)vcdGrow(writer->sigs, &writer->sigCap, writer->sigCount + 1u, sizeof *writer->sigs);
    if (sigs == NULL)
    {
        writer->err = VCD_ERR_NOMEM;
        return writer->err;
    }
    writer->sigs = sigs;
    s = &writer->sigs[writer->sigCount];
    memset(s, 0, sizeof *s);
    s->type = type;
    s->width = width;
    (void)vcdEncodeId((uint32_t)writer->sigCount, s->id);

    (void)snprintf(line, sizeof line, "$var %s %u %s ", VCD_VarTypeStr(type), (unsigned)width, s->id);
    outStr(writer, line);
    outStr(writer, name);
    outStr(writer, " $end\n");

    if (signal != NULL)
    {
        *signal = (vcdSignal_t)writer->sigCount;
    }
    writer->sigCount++;
    return writer->err;
}

vcdStatus_t VCD_ChangeBit(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, char value)
{
    sigInfo_t *s;
    char tok[2];
    vcdStatus_t st = lookupSignal(writer, signal, &s);

    if (st != VCD_OK)
    {
        return st;
    }
    if (!isScalar(s))
    {
        return VCD_ERR_ARG;
    }
    tok[0] = normBit(value);
    tok[1] = '\0';
    if (tok[0] == '\0')
    {
        return VCD_ERR_VALUE;
    }
    return addChange(writer, signal, time, tok, 1u);
}

vcdStatus_t VCD_ChangeBool(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, bool value)
{
    return VCD_ChangeBit(writer, signal, time, value ? '1' : '0');
}

vcdStatus_t VCD_ChangeU64(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, uint64_t value)
{
    sigInfo_t *s;
    char tok[2u + 64u];
    size_t len = 0u;
    int bit;
    vcdStatus_t st = lookupSignal(writer, signal, &s);

    if (st != VCD_OK)
    {
        return st;
    }
    if (!isDigital(s->type))
    {
        return VCD_ERR_ARG;
    }
    if ((s->width < 64u) && ((value >> s->width) != 0u))
    {
        return VCD_ERR_VALUE;
    }

    if (isScalar(s))
    {
        tok[len++] = (value != 0u) ? '1' : '0';
    }
    else
    {
        tok[len++] = 'b';
        for (bit = 63; (bit > 0) && (((value >> bit) & 1u) == 0u); bit--)
        {
        }
        for (; bit >= 0; bit--)
        {
            tok[len++] = (((value >> bit) & 1u) != 0u) ? '1' : '0';
        }
    }
    tok[len] = '\0';
    return addChange(writer, signal, time, tok, len);
}

vcdStatus_t VCD_ChangeBits(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, const char *bits)
{
    sigInfo_t *s;
    char stackTok[STACK_TOKEN_LEN];
    char *tok = stackTok;
    size_t n;
    size_t i;
    size_t start = 0u;
    vcdStatus_t st = lookupSignal(writer, signal, &s);

    if (st != VCD_OK)
    {
        return st;
    }
    if ((bits == NULL) || !isDigital(s->type))
    {
        return VCD_ERR_ARG;
    }
    n = strlen(bits);
    if ((n == 0u) || (n > s->width))
    {
        return VCD_ERR_VALUE;
    }
    for (i = 0u; i < n; i++)
    {
        if (normBit(bits[i]) == '\0')
        {
            return VCD_ERR_VALUE;
        }
    }

    if (isScalar(s))
    {
        stackTok[0] = normBit(bits[0]);
        return addChange(writer, signal, time, stackTok, 1u);
    }

    /* Drop leading digits that the VCD left-extension rule recreates anyway, so equal values compare
     * equal for dropRedundant: "0001x" -> "1x", "xxx1" -> "x1", "0000" -> "0". */
    while ((n - start) > 1u)
    {
        char c = normBit(bits[start]);
        char next = normBit(bits[start + 1u]);
        bool zeroRun = (c == '0') && ((next == '0') || (next == '1'));
        bool xzRun = ((c == 'x') || (c == 'z')) && (next == c);
        if (!zeroRun && !xzRun)
        {
            break;
        }
        start++;
    }

    if ((n - start + 2u) > STACK_TOKEN_LEN)
    {
        tok = (char *)VCD_MALLOC(n - start + 2u);
        if (tok == NULL)
        {
            return VCD_ERR_NOMEM;
        }
    }
    tok[0] = 'b';
    for (i = start; i < n; i++)
    {
        tok[1u + i - start] = normBit(bits[i]);
    }
    tok[1u + n - start] = '\0';

    st = addChange(writer, signal, time, tok, 1u + n - start);
    if (tok != stackTok)
    {
        VCD_FREE(tok);
    }
    return st;
}

vcdStatus_t VCD_ChangeReal(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, double value)
{
    sigInfo_t *s;
    char tok[40];
    int n;
    vcdStatus_t st = lookupSignal(writer, signal, &s);

    if (st != VCD_OK)
    {
        return st;
    }
    if (s->type != VCD_VAR_REAL)
    {
        return VCD_ERR_ARG;
    }
    if (!isfinite(value))
    {
        return VCD_ERR_VALUE;
    }
    n = snprintf(tok, sizeof tok, "r%.17g", value);
    return addChange(writer, signal, time, tok, (size_t)n);
}

vcdStatus_t VCD_ChangeString(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, const char *text)
{
    sigInfo_t *s;
    char stackTok[STACK_TOKEN_LEN];
    char *tok = stackTok;
    size_t n;
    size_t i;
    vcdStatus_t st = lookupSignal(writer, signal, &s);

    if (st != VCD_OK)
    {
        return st;
    }
    if ((text == NULL) || (s->type != VCD_VAR_STRING))
    {
        return VCD_ERR_ARG;
    }

    n = strlen(text);
    if ((n + 2u) > STACK_TOKEN_LEN)
    {
        tok = (char *)VCD_MALLOC(n + 2u);
        if (tok == NULL)
        {
            return VCD_ERR_NOMEM;
        }
    }
    tok[0] = 's';
    for (i = 0u; i < n; i++)
    {
        unsigned char c = (unsigned char)text[i];
        tok[i + 1u] = ((c <= (unsigned char)' ') || (c == 0x7Fu)) ? '_' : (char)c;
    }
    tok[n + 1u] = '\0';

    st = addChange(writer, signal, time, tok, n + 1u);
    if (tok != stackTok)
    {
        VCD_FREE(tok);
    }
    return st;
}

vcdStatus_t VCD_Pulse(vcdWriter_t *writer, vcdSignal_t signal, vcdTime_t time, vcdTime_t width)
{
    vcdStatus_t st;

    if ((width == 0u) || (time >= (VCD_TIME_NONE - width)))
    {
        return VCD_ERR_ARG;
    }
    st = VCD_ChangeBit(writer, signal, time, '1');
    if (st == VCD_OK)
    {
        st = VCD_ChangeBit(writer, signal, time + width, '0');
    }
    return st;
}

vcdStatus_t VCD_Commit(vcdWriter_t *writer, vcdTime_t time)
{
    vcdStatus_t st = checkWriter(writer);

    if (st != VCD_OK)
    {
        return st;
    }
    if (time > writer->watermark)
    {
        emitBefore(writer, time);
        writer->watermark = time;
    }
    return writer->err;
}

vcdStatus_t VCD_Flush(vcdWriter_t *writer)
{
    vcdStatus_t st = checkWriter(writer);

    if (st != VCD_OK)
    {
        return st;
    }
    outFlush(writer);
    if ((writer->file != NULL) && (fflush(writer->file) != 0) && (writer->err == VCD_OK))
    {
        writer->err = VCD_ERR_IO;
    }
    return writer->err;
}

vcdStatus_t VCD_WriterStatus(const vcdWriter_t *writer)
{
    return checkWriter(writer);
}

vcdTimescale_t VCD_WriterTimescale(const vcdWriter_t *writer)
{
    vcdTimescale_t none = {0u, VCD_UNIT_S};
    return (writer != NULL) ? writer->timescale : none;
}
