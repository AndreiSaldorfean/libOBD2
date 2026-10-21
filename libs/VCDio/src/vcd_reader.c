/* ================================================ INCLUDES =============================================== */
#include "vcd_reader.h"
#include "vcd_internal.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define READ_CHUNK        65536u
#define VAR_TOKENS_MAX    16u
#define SCOPE_TOKENS_MAX  4u
#define TIMESCALE_TOKENS  4u
#define TIMESCALE_TEXT    32u
#define ID_SLOTS_MIN      64u
#define SCALAR_CHARS      "01xXzZuUwWlLhH-"
#define ERR_TOKEN_PREVIEW 40

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    const char *p;
    size_t      n;
} tok_t;

typedef struct
{
    vcdTime_t time;
    uint32_t  val; /* offset of the value text in the pool */
} change_t;

/* One identifier code = one stream of changes, shared by all aliased variables. */
typedef struct
{
    char     *id;
    size_t    idLen;
    change_t *ch;
    size_t    count;
    size_t    cap;
} trace_t;

typedef struct
{
    vcdVarInfo_t info;
    char        *name;
    char        *scope;
    char        *fullName;
    char        *typeName;
    uint32_t     trace;
} var_t;

struct vcdReader
{
    /* parse cursor, only valid while opening */
    const char      *buf;
    size_t           len;
    size_t           pos;
    size_t           line;
    vcdParseError_t *err;

    vcdTimescale_t timescale;
    char          *date;
    char          *version;
    char          *comment;

    var_t   *vars;
    size_t   varCount;
    size_t   varCap;
    trace_t *traces;
    size_t   traceCount;
    size_t   traceCap;
    uint32_t *slots; /* open addressing id -> trace index + 1 */
    size_t    slotCap;

    char    *pool;
    size_t   poolLen;
    size_t   poolCap;
    uint32_t scalarOff[256]; /* pool offset of each single-character value, 0 = not added yet */

    char   *scope;
    size_t  scopeLen;
    size_t  scopeCap;
    size_t *scopeStack;
    size_t  scopeDepth;
    size_t  scopeStackCap;

    vcdTime_t now;
    bool      timeSeen;
    vcdTime_t startTime;
    vcdTime_t endTime;
};

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static vcdStatus_t parseError(vcdReader_t *r, const char *fmt, ...) VCD_PRINTF_FMT(2, 3);
static bool nextToken(vcdReader_t *r, tok_t *t);
static bool tokIs(tok_t t, const char *s);
static bool parseU64(const char *p, size_t n, uint64_t *out);
static uint32_t hashId(const char *p, size_t n);
static void slotPlace(uint32_t *slots, size_t cap, const trace_t *traces, uint32_t trace);
static bool idLookup(const vcdReader_t *r, const char *p, size_t n, uint32_t *trace);
static vcdStatus_t traceCreate(vcdReader_t *r, const char *p, size_t n, uint32_t *trace);
static vcdStatus_t poolAdd(vcdReader_t *r, const char *s, size_t n, bool lower, uint32_t *off);
static vcdStatus_t collectUntilEnd(vcdReader_t *r, const char *kw, tok_t *toks, size_t max, size_t *count);
static vcdStatus_t readText(vcdReader_t *r, const char *kw, char **out);
static vcdStatus_t parseTimescale(vcdReader_t *r);
static vcdStatus_t pushScope(vcdReader_t *r, const char *p, size_t n);
static vcdVarType_t mapType(tok_t t);
static vcdStatus_t addVar(vcdReader_t *r, const tok_t *toks, size_t count);
static vcdStatus_t addChange(vcdReader_t *r, tok_t id, const char *val, size_t n, bool lower);
static vcdStatus_t parseKeyword(vcdReader_t *r, tok_t t);
static vcdStatus_t parseTime(vcdReader_t *r, tok_t t);
static vcdStatus_t parse(vcdReader_t *r);
static vcdStatus_t loadFile(const char *path, char **data, size_t *len);
static const trace_t *varTrace(const vcdReader_t *r, size_t var);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static vcdStatus_t parseError(vcdReader_t *r, const char *fmt, ...)
{
    if (r->err != NULL)
    {
        va_list ap;
        va_start(ap, fmt);
        (void)vsnprintf(r->err->message, sizeof r->err->message, fmt, ap);
        va_end(ap);
        r->err->line = r->line;
    }
    return VCD_ERR_PARSE;
}

static bool nextToken(vcdReader_t *r, tok_t *t)
{
    size_t start;

    while ((r->pos < r->len) && isspace((unsigned char)r->buf[r->pos]))
    {
        if (r->buf[r->pos] == '\n')
        {
            r->line++;
        }
        r->pos++;
    }
    if (r->pos >= r->len)
    {
        return false;
    }

    start = r->pos;
    while ((r->pos < r->len) && !isspace((unsigned char)r->buf[r->pos]))
    {
        r->pos++;
    }
    t->p = r->buf + start;
    t->n = r->pos - start;
    return true;
}

static bool tokIs(tok_t t, const char *s)
{
    size_t n = strlen(s);
    return (t.n == n) && (memcmp(t.p, s, n) == 0);
}

static bool parseU64(const char *p, size_t n, uint64_t *out)
{
    uint64_t v = 0u;
    size_t i;

    if (n == 0u)
    {
        return false;
    }
    for (i = 0u; i < n; i++)
    {
        uint64_t digit;
        if ((p[i] < '0') || (p[i] > '9'))
        {
            return false;
        }
        digit = (uint64_t)(p[i] - '0');
        if (v > ((UINT64_MAX - digit) / 10u))
        {
            return false;
        }
        v = (v * 10u) + digit;
    }
    *out = v;
    return true;
}

static uint32_t hashId(const char *p, size_t n)
{
    uint32_t h = 2166136261u; /* FNV-1a */
    size_t i;

    for (i = 0u; i < n; i++)
    {
        h ^= (uint8_t)p[i];
        h *= 16777619u;
    }
    return h;
}

static void slotPlace(uint32_t *slots, size_t cap, const trace_t *traces, uint32_t trace)
{
    size_t mask = cap - 1u;
    size_t i = hashId(traces[trace].id, traces[trace].idLen) & mask;

    while (slots[i] != 0u)
    {
        i = (i + 1u) & mask;
    }
    slots[i] = trace + 1u;
}

static bool idLookup(const vcdReader_t *r, const char *p, size_t n, uint32_t *trace)
{
    size_t mask;
    size_t i;

    if (r->slotCap == 0u)
    {
        return false;
    }
    mask = r->slotCap - 1u;
    for (i = hashId(p, n) & mask; r->slots[i] != 0u; i = (i + 1u) & mask)
    {
        const trace_t *t = &r->traces[r->slots[i] - 1u];
        if ((t->idLen == n) && (memcmp(t->id, p, n) == 0))
        {
            *trace = r->slots[i] - 1u;
            return true;
        }
    }
    return false;
}

static vcdStatus_t traceCreate(vcdReader_t *r, const char *p, size_t n, uint32_t *trace)
{
    trace_t *traces;
    trace_t *t;
    uint32_t idx;

    if (r->traceCount >= (size_t)(UINT32_MAX - 1u))
    {
        return VCD_ERR_NOMEM;
    }
    traces = (trace_t *)vcdGrow(r->traces, &r->traceCap, r->traceCount + 1u, sizeof *r->traces);
    if (traces == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    r->traces = traces;
    t = &r->traces[r->traceCount];
    memset(t, 0, sizeof *t);
    t->id = vcdStrNDup(p, n);
    if (t->id == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    t->idLen = n;
    idx = (uint32_t)r->traceCount;
    r->traceCount++;

    /* keep the table at most half full */
    if ((r->traceCount * 2u) > r->slotCap)
    {
        size_t cap = (r->slotCap != 0u) ? (r->slotCap * 2u) : ID_SLOTS_MIN;
        uint32_t *slots = (uint32_t *)VCD_MALLOC(cap * sizeof *slots);
        size_t i;

        if (slots == NULL)
        {
            r->traceCount--;
            VCD_FREE(t->id);
            return VCD_ERR_NOMEM;
        }
        memset(slots, 0, cap * sizeof *slots);
        for (i = 0u; i < r->traceCount; i++)
        {
            slotPlace(slots, cap, r->traces, (uint32_t)i);
        }
        VCD_FREE(r->slots);
        r->slots = slots;
        r->slotCap = cap;
    }
    else
    {
        slotPlace(r->slots, r->slotCap, r->traces, idx);
    }

    *trace = idx;
    return VCD_OK;
}

static vcdStatus_t poolAdd(vcdReader_t *r, const char *s, size_t n, bool lower, uint32_t *off)
{
    char *pool;
    char *dst;
    size_t i;

    if ((n + 1u) > ((size_t)UINT32_MAX - r->poolLen))
    {
        return VCD_ERR_NOMEM;
    }
    pool = (char *)vcdGrow(r->pool, &r->poolCap, r->poolLen + n + 1u, 1u);
    if (pool == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    r->pool = pool;

    dst = r->pool + r->poolLen;
    for (i = 0u; i < n; i++)
    {
        dst[i] = lower ? (char)tolower((unsigned char)s[i]) : s[i];
    }
    dst[n] = '\0';
    *off = (uint32_t)r->poolLen;
    r->poolLen += n + 1u;
    return VCD_OK;
}

static vcdStatus_t collectUntilEnd(vcdReader_t *r, const char *kw, tok_t *toks, size_t max, size_t *count)
{
    size_t n = 0u;
    tok_t t;

    for (;;)
    {
        if (!nextToken(r, &t))
        {
            return parseError(r, "missing $end after %s", kw);
        }
        if (tokIs(t, "$end"))
        {
            break;
        }
        if (n >= max)
        {
            return parseError(r, "too many words in %s", kw);
        }
        toks[n++] = t;
    }
    *count = n;
    return VCD_OK;
}

/* Reads the free text of a section up to $end. With out == NULL the text is skipped. */
static vcdStatus_t readText(vcdReader_t *r, const char *kw, char **out)
{
    const char *first = NULL;
    const char *last = NULL;
    char *text;
    tok_t t;

    for (;;)
    {
        if (!nextToken(r, &t))
        {
            return parseError(r, "missing $end after %s", kw);
        }
        if (tokIs(t, "$end"))
        {
            break;
        }
        if (first == NULL)
        {
            first = t.p;
        }
        last = t.p + t.n;
    }
    if (out == NULL)
    {
        return VCD_OK;
    }

    text = (first != NULL) ? vcdStrNDup(first, (size_t)(last - first)) : vcdStrDup("");
    if (text == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    VCD_FREE(*out);
    *out = text;
    return VCD_OK;
}

static vcdStatus_t parseTimescale(vcdReader_t *r)
{
    static const char *const units[] = {"s", "ms", "us", "ns", "ps", "fs"};
    tok_t toks[TIMESCALE_TOKENS];
    char text[TIMESCALE_TEXT];
    size_t count;
    size_t len = 0u;
    size_t i;
    uint64_t mag;
    vcdStatus_t st = collectUntilEnd(r, "$timescale", toks, TIMESCALE_TOKENS, &count);

    if (st != VCD_OK)
    {
        return st;
    }
    for (i = 0u; i < count; i++)
    {
        if ((len + toks[i].n) >= sizeof text)
        {
            return parseError(r, "invalid $timescale");
        }
        memcpy(text + len, toks[i].p, toks[i].n);
        len += toks[i].n;
    }
    text[len] = '\0';

    for (i = 0u; (i < len) && isdigit((unsigned char)text[i]); i++)
    {
    }
    if (!parseU64(text, i, &mag) || (mag == 0u) || (mag > UINT16_MAX))
    {
        return parseError(r, "invalid $timescale '%s'", text);
    }
    for (size_t u = 0u; u < (sizeof units / sizeof units[0]); u++)
    {
        if (strcmp(text + i, units[u]) == 0)
        {
            r->timescale.magnitude = (uint16_t)mag;
            r->timescale.unit = (vcdUnit_t)u;
            return VCD_OK;
        }
    }
    return parseError(r, "invalid $timescale '%s'", text);
}

static vcdStatus_t pushScope(vcdReader_t *r, const char *p, size_t n)
{
    size_t *stack;
    char *scope;
    size_t need = r->scopeLen + 1u + n + 1u;

    stack = (size_t *)vcdGrow(r->scopeStack, &r->scopeStackCap, r->scopeDepth + 1u, sizeof *r->scopeStack);
    if (stack == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    r->scopeStack = stack;
    scope = (char *)vcdGrow(r->scope, &r->scopeCap, need, 1u);
    if (scope == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    r->scope = scope;

    r->scopeStack[r->scopeDepth++] = r->scopeLen;
    if (r->scopeLen > 0u)
    {
        r->scope[r->scopeLen++] = '.';
    }
    memcpy(r->scope + r->scopeLen, p, n);
    r->scopeLen += n;
    r->scope[r->scopeLen] = '\0';
    return VCD_OK;
}

static vcdVarType_t mapType(tok_t t)
{
    static const struct
    {
        const char  *name;
        vcdVarType_t type;
    } map[] = {
        {"wire", VCD_VAR_WIRE},       {"reg", VCD_VAR_REG},           {"logic", VCD_VAR_REG},
        {"bit", VCD_VAR_REG},         {"integer", VCD_VAR_INTEGER},   {"int", VCD_VAR_INTEGER},
        {"shortint", VCD_VAR_INTEGER}, {"longint", VCD_VAR_INTEGER},  {"byte", VCD_VAR_INTEGER},
        {"time", VCD_VAR_INTEGER},    {"real", VCD_VAR_REAL},         {"realtime", VCD_VAR_REAL},
        {"shortreal", VCD_VAR_REAL},  {"string", VCD_VAR_STRING},     {"event", VCD_VAR_EVENT},
        {"parameter", VCD_VAR_PARAMETER},
    };
    size_t i;

    for (i = 0u; i < (sizeof map / sizeof map[0]); i++)
    {
        if (tokIs(t, map[i].name))
        {
            return map[i].type;
        }
    }
    return VCD_VAR_OTHER;
}

/* $var <type> <width> <id> <reference> [bit range ...] */
static vcdStatus_t addVar(vcdReader_t *r, const tok_t *toks, size_t count)
{
    var_t *vars;
    var_t *v;
    uint64_t width;
    uint32_t trace;
    size_t nameLen = 0u;
    size_t i;
    vcdStatus_t st;

    if (count < 4u)
    {
        return parseError(r, "$var needs a type, width, identifier and name");
    }
    if (!parseU64(toks[1].p, toks[1].n, &width) || (width == 0u) || (width > UINT32_MAX))
    {
        return parseError(r, "invalid $var width '%.*s'", (int)toks[1].n, toks[1].p);
    }
    if (!idLookup(r, toks[2].p, toks[2].n, &trace))
    {
        st = traceCreate(r, toks[2].p, toks[2].n, &trace);
        if (st != VCD_OK)
        {
            return st;
        }
    }

    vars = (var_t *)vcdGrow(r->vars, &r->varCap, r->varCount + 1u, sizeof *r->vars);
    if (vars == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    r->vars = vars;
    v = &r->vars[r->varCount];
    memset(v, 0, sizeof *v);
    r->varCount++; /* counted now so VCD_ReaderClose() frees partial allocations */

    for (i = 3u; i < count; i++)
    {
        nameLen += toks[i].n;
    }
    v->name = (char *)VCD_MALLOC(nameLen + 1u);
    v->typeName = vcdStrNDup(toks[0].p, toks[0].n);
    v->scope = vcdStrDup((r->scope != NULL) ? r->scope : "");
    v->fullName = (char *)VCD_MALLOC(r->scopeLen + 1u + nameLen + 1u);
    if ((v->name == NULL) || (v->typeName == NULL) || (v->scope == NULL) || (v->fullName == NULL))
    {
        return VCD_ERR_NOMEM;
    }

    nameLen = 0u;
    for (i = 3u; i < count; i++)
    {
        memcpy(v->name + nameLen, toks[i].p, toks[i].n);
        nameLen += toks[i].n;
    }
    v->name[nameLen] = '\0';
    if (r->scopeLen > 0u)
    {
        (void)sprintf(v->fullName, "%s.%s", v->scope, v->name);
    }
    else
    {
        (void)strcpy(v->fullName, v->name);
    }

    v->trace = trace;
    v->info.name = v->name;
    v->info.scope = v->scope;
    v->info.fullName = v->fullName;
    v->info.typeName = v->typeName;
    v->info.id = r->traces[trace].id;
    v->info.type = mapType(toks[0]);
    v->info.width = (uint32_t)width;
    return VCD_OK;
}

static vcdStatus_t addChange(vcdReader_t *r, tok_t id, const char *val, size_t n, bool lower)
{
    change_t *ch;
    trace_t *t;
    uint32_t trace;
    uint32_t off;
    vcdStatus_t st;

    if (!idLookup(r, id.p, id.n, &trace))
    {
        return parseError(r, "unknown identifier '%.*s'", (int)id.n, id.p);
    }

    if (n == 1u)
    {
        unsigned char c = lower ? (unsigned char)tolower((unsigned char)val[0]) : (unsigned char)val[0];
        if (r->scalarOff[c] == 0u)
        {
            st = poolAdd(r, (const char *)&c, 1u, false, &r->scalarOff[c]);
            if (st != VCD_OK)
            {
                return st;
            }
        }
        off = r->scalarOff[c];
    }
    else
    {
        st = poolAdd(r, val, n, lower, &off);
        if (st != VCD_OK)
        {
            return st;
        }
    }

    t = &r->traces[trace];
    ch = (change_t *)vcdGrow(t->ch, &t->cap, t->count + 1u, sizeof *t->ch);
    if (ch == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    t->ch = ch;
    t->ch[t->count].time = r->now;
    t->ch[t->count].val = off;
    t->count++;
    return VCD_OK;
}

static vcdStatus_t parseKeyword(vcdReader_t *r, tok_t t)
{
    tok_t toks[VAR_TOKENS_MAX];
    size_t count;
    vcdStatus_t st;

    /* $end closes $dumpvars/$dumpall/$dumpon/$dumpoff, whose values are ordinary changes */
    if (tokIs(t, "$end") || tokIs(t, "$dumpvars") || tokIs(t, "$dumpall") || tokIs(t, "$dumpon") ||
        tokIs(t, "$dumpoff"))
    {
        return VCD_OK;
    }
    if (tokIs(t, "$date"))
    {
        return readText(r, "$date", &r->date);
    }
    if (tokIs(t, "$version"))
    {
        return readText(r, "$version", &r->version);
    }
    if (tokIs(t, "$comment"))
    {
        return readText(r, "$comment", &r->comment);
    }
    if (tokIs(t, "$timescale"))
    {
        return parseTimescale(r);
    }
    if (tokIs(t, "$scope"))
    {
        st = collectUntilEnd(r, "$scope", toks, SCOPE_TOKENS_MAX, &count);
        if (st != VCD_OK)
        {
            return st;
        }
        if (count == 0u)
        {
            return pushScope(r, "", 0u);
        }
        return pushScope(r, toks[count - 1u].p, toks[count - 1u].n);
    }
    if (tokIs(t, "$upscope"))
    {
        st = collectUntilEnd(r, "$upscope", toks, SCOPE_TOKENS_MAX, &count);
        if ((st == VCD_OK) && (r->scopeDepth > 0u))
        {
            r->scopeLen = r->scopeStack[--r->scopeDepth];
            r->scope[r->scopeLen] = '\0';
        }
        return st;
    }
    if (tokIs(t, "$var"))
    {
        st = collectUntilEnd(r, "$var", toks, VAR_TOKENS_MAX, &count);
        return (st == VCD_OK) ? addVar(r, toks, count) : st;
    }
    /* $enddefinitions and unknown sections: skip to $end */
    return readText(r, "section", NULL);
}

static vcdStatus_t parseTime(vcdReader_t *r, tok_t t)
{
    uint64_t v;

    if (!parseU64(t.p + 1, t.n - 1u, &v))
    {
        return parseError(r, "invalid time '%.*s'", (int)((t.n < ERR_TOKEN_PREVIEW) ? t.n : ERR_TOKEN_PREVIEW), t.p);
    }
    if (r->timeSeen && (v < r->now))
    {
        return parseError(r, "time goes backwards (#%" PRIu64 " after #%" PRIu64 ")", v, r->now);
    }
    if (!r->timeSeen)
    {
        r->startTime = v;
        r->timeSeen = true;
    }
    r->now = v;
    r->endTime = v;
    return VCD_OK;
}

static vcdStatus_t parse(vcdReader_t *r)
{
    vcdStatus_t st = VCD_OK;
    tok_t t;

    while ((st == VCD_OK) && nextToken(r, &t))
    {
        const char c = t.p[0];

        if (c == '$')
        {
            st = parseKeyword(r, t);
        }
        else if (c == '#')
        {
            st = parseTime(r, t);
        }
        else if ((c == 'b') || (c == 'B') || (c == 'r') || (c == 'R') || (c == 's') || (c == 'S'))
        {
            bool isString = (c == 's') || (c == 'S');
            bool isVector = (c == 'b') || (c == 'B');
            tok_t id;

            if ((t.n < 2u) && !isString)
            {
                return parseError(r, "missing value after '%c'", c);
            }
            if (!nextToken(r, &id))
            {
                return parseError(r, "missing identifier after value '%.*s'", (int)t.n, t.p);
            }
            st = addChange(r, id, t.p + 1, t.n - 1u, isVector);
        }
        else if ((c != '\0') && (strchr(SCALAR_CHARS, c) != NULL))
        {
            tok_t id;
            if (t.n < 2u)
            {
                return parseError(r, "missing identifier after value '%c'", c);
            }
            id.p = t.p + 1;
            id.n = t.n - 1u;
            st = addChange(r, id, t.p, 1u, true);
        }
        else
        {
            int n = (int)((t.n < ERR_TOKEN_PREVIEW) ? t.n : ERR_TOKEN_PREVIEW);
            st = parseError(r, "unexpected '%.*s'", n, t.p);
        }
    }
    return st;
}

static vcdStatus_t loadFile(const char *path, char **data, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf = NULL;
    size_t cap = 0u;
    size_t n = 0u;

    if (f == NULL)
    {
        return VCD_ERR_IO;
    }
    for (;;)
    {
        size_t got;
        char *p = (char *)vcdGrow(buf, &cap, n + READ_CHUNK, 1u);
        if (p == NULL)
        {
            VCD_FREE(buf);
            (void)fclose(f);
            return VCD_ERR_NOMEM;
        }
        buf = p;
        got = fread(buf + n, 1u, READ_CHUNK, f);
        n += got;
        if (got < READ_CHUNK)
        {
            break;
        }
    }
    if (ferror(f))
    {
        VCD_FREE(buf);
        (void)fclose(f);
        return VCD_ERR_IO;
    }
    (void)fclose(f);
    *data = buf;
    *len = n;
    return VCD_OK;
}

static const trace_t *varTrace(const vcdReader_t *r, size_t var)
{
    if ((r == NULL) || (var >= r->varCount))
    {
        return NULL;
    }
    return &r->traces[r->vars[var].trace];
}

/* ================================================ MODULE API ============================================= */
vcdStatus_t VCD_ReaderOpenMemory(vcdReader_t **reader, const char *data, size_t len, vcdParseError_t *err)
{
    vcdReader_t *r;
    uint32_t emptyOff;
    vcdStatus_t st;

    if (err != NULL)
    {
        err->line = 0u;
        err->message[0] = '\0';
    }
    if ((reader == NULL) || ((data == NULL) && (len > 0u)))
    {
        return VCD_ERR_ARG;
    }
    *reader = NULL;

    r = (vcdReader_t *)VCD_MALLOC(sizeof *r);
    if (r == NULL)
    {
        return VCD_ERR_NOMEM;
    }
    memset(r, 0, sizeof *r);
    r->buf = data;
    r->len = len;
    r->line = 1u;
    r->err = err;
    r->timescale.magnitude = 1u;
    r->timescale.unit = VCD_UNIT_NS;

    st = poolAdd(r, "", 0u, false, &emptyOff); /* offset 0 = "", so scalarOff can use 0 as "unset" */
    if (st == VCD_OK)
    {
        st = parse(r);
    }
    r->buf = NULL;
    r->err = NULL;

    if (st != VCD_OK)
    {
        if ((err != NULL) && (err->message[0] == '\0'))
        {
            (void)snprintf(err->message, sizeof err->message, "%s", VCD_StatusStr(st));
        }
        VCD_ReaderClose(r);
        return st;
    }
    *reader = r;
    return VCD_OK;
}

vcdStatus_t VCD_ReaderOpenFile(vcdReader_t **reader, const char *path, vcdParseError_t *err)
{
    char *data = NULL;
    size_t len = 0u;
    vcdStatus_t st;

    if (err != NULL)
    {
        err->line = 0u;
        err->message[0] = '\0';
    }
    if ((reader == NULL) || (path == NULL))
    {
        return VCD_ERR_ARG;
    }
    *reader = NULL;

    st = loadFile(path, &data, &len);
    if (st != VCD_OK)
    {
        if (err != NULL)
        {
            (void)snprintf(err->message, sizeof err->message, "cannot read '%s'", path);
        }
        return st;
    }
    st = VCD_ReaderOpenMemory(reader, data, len, err);
    VCD_FREE(data);
    return st;
}

void VCD_ReaderClose(vcdReader_t *reader)
{
    size_t i;

    if (reader == NULL)
    {
        return;
    }
    for (i = 0u; i < reader->varCount; i++)
    {
        VCD_FREE(reader->vars[i].name);
        VCD_FREE(reader->vars[i].scope);
        VCD_FREE(reader->vars[i].fullName);
        VCD_FREE(reader->vars[i].typeName);
    }
    for (i = 0u; i < reader->traceCount; i++)
    {
        VCD_FREE(reader->traces[i].id);
        VCD_FREE(reader->traces[i].ch);
    }
    VCD_FREE(reader->vars);
    VCD_FREE(reader->traces);
    VCD_FREE(reader->slots);
    VCD_FREE(reader->pool);
    VCD_FREE(reader->scope);
    VCD_FREE(reader->scopeStack);
    VCD_FREE(reader->date);
    VCD_FREE(reader->version);
    VCD_FREE(reader->comment);
    VCD_FREE(reader);
}

vcdTimescale_t VCD_ReaderTimescale(const vcdReader_t *reader)
{
    vcdTimescale_t none = {0u, VCD_UNIT_S};
    return (reader != NULL) ? reader->timescale : none;
}

const char *VCD_ReaderDate(const vcdReader_t *reader)
{
    return (reader != NULL) ? reader->date : NULL;
}

const char *VCD_ReaderVersion(const vcdReader_t *reader)
{
    return (reader != NULL) ? reader->version : NULL;
}

const char *VCD_ReaderComment(const vcdReader_t *reader)
{
    return (reader != NULL) ? reader->comment : NULL;
}

vcdTime_t VCD_ReaderStartTime(const vcdReader_t *reader)
{
    return (reader != NULL) ? reader->startTime : 0u;
}

vcdTime_t VCD_ReaderEndTime(const vcdReader_t *reader)
{
    return (reader != NULL) ? reader->endTime : 0u;
}

size_t VCD_ReaderVarCount(const vcdReader_t *reader)
{
    return (reader != NULL) ? reader->varCount : 0u;
}

const vcdVarInfo_t *VCD_ReaderVar(const vcdReader_t *reader, size_t var)
{
    if ((reader == NULL) || (var >= reader->varCount))
    {
        return NULL;
    }
    return &reader->vars[var].info;
}

vcdStatus_t VCD_ReaderFind(const vcdReader_t *reader, const char *name, size_t *var)
{
    size_t found = SIZE_MAX;
    size_t nameLen;
    size_t i;

    if ((reader == NULL) || (name == NULL) || (var == NULL))
    {
        return VCD_ERR_ARG;
    }

    for (i = 0u; i < reader->varCount; i++)
    {
        if (strcmp(reader->vars[i].fullName, name) == 0)
        {
            *var = i;
            return VCD_OK;
        }
    }

    nameLen = strlen(name);
    for (i = 0u; i < reader->varCount; i++)
    {
        const char *leaf = reader->vars[i].name;
        bool exact = strcmp(leaf, name) == 0;
        bool withoutRange = (strncmp(leaf, name, nameLen) == 0) && (leaf[nameLen] == '[');

        if (!exact && !withoutRange)
        {
            continue;
        }
        if (found == SIZE_MAX)
        {
            found = i;
        }
        else if (reader->vars[i].trace != reader->vars[found].trace)
        {
            return VCD_ERR_ARG;
        }
    }

    if (found == SIZE_MAX)
    {
        return VCD_ERR_NOT_FOUND;
    }
    *var = found;
    return VCD_OK;
}

size_t VCD_ReaderChangeCount(const vcdReader_t *reader, size_t var)
{
    const trace_t *t = varTrace(reader, var);
    return (t != NULL) ? t->count : 0u;
}

vcdStatus_t VCD_ReaderChange(
    const vcdReader_t *reader,
    size_t var,
    size_t index,
    vcdTime_t *time,
    const char **value)
{
    const trace_t *t = varTrace(reader, var);

    if ((t == NULL) || (index >= t->count))
    {
        return VCD_ERR_ARG;
    }
    if (time != NULL)
    {
        *time = t->ch[index].time;
    }
    if (value != NULL)
    {
        *value = reader->pool + t->ch[index].val;
    }
    return VCD_OK;
}

vcdStatus_t VCD_ReaderFindChange(const vcdReader_t *reader, size_t var, vcdTime_t time, size_t *index)
{
    const trace_t *t = varTrace(reader, var);
    size_t lo = 0u;
    size_t hi;

    if ((t == NULL) || (index == NULL))
    {
        return VCD_ERR_ARG;
    }

    /* first change later than 'time' */
    hi = t->count;
    while (lo < hi)
    {
        size_t mid = lo + ((hi - lo) / 2u);
        if (t->ch[mid].time <= time)
        {
            lo = mid + 1u;
        }
        else
        {
            hi = mid;
        }
    }
    if (lo == 0u)
    {
        return VCD_ERR_NOT_FOUND;
    }
    *index = lo - 1u;
    return VCD_OK;
}

const char *VCD_ReaderValueAt(const vcdReader_t *reader, size_t var, vcdTime_t time)
{
    size_t index;

    if (VCD_ReaderFindChange(reader, var, time, &index) != VCD_OK)
    {
        return NULL;
    }
    return reader->pool + reader->traces[reader->vars[var].trace].ch[index].val;
}
