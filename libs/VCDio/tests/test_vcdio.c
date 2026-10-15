/*************************************************************************************************************
 * VCDio unit tests. Self-contained: no test framework needed, exit code 0 on success.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "vcd_internal.h"
#include "vcd_io.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define CHECK(cond)                                                                                         \
    do                                                                                                      \
    {                                                                                                       \
        g_checks++;                                                                                         \
        if (!(cond))                                                                                        \
        {                                                                                                   \
            g_failed++;                                                                                     \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);                         \
        }                                                                                                   \
    } while (0)

#define CHECK_ST(expr, expected)                                                                            \
    do                                                                                                      \
    {                                                                                                       \
        vcdStatus_t st_ = (expr);                                                                           \
        g_checks++;                                                                                         \
        if (st_ != (expected))                                                                              \
        {                                                                                                   \
            g_failed++;                                                                                     \
            fprintf(stderr, "%s:%d: %s returned '%s', expected '%s'\n", __FILE__, __LINE__, #expr,          \
                    VCD_StatusStr(st_), VCD_StatusStr(expected));                                           \
        }                                                                                                   \
    } while (0)

#define CHECK_OK(expr) CHECK_ST((expr), VCD_OK)

#define CHECK_STR(actual, expected)                                                                         \
    do                                                                                                      \
    {                                                                                                       \
        const char *a_ = (actual);                                                                          \
        const char *e_ = (expected);                                                                        \
        g_checks++;                                                                                         \
        if ((a_ == NULL) || (strcmp(a_, e_) != 0))                                                          \
        {                                                                                                   \
            g_failed++;                                                                                     \
            fprintf(stderr, "%s:%d: got \"%s\", expected \"%s\"\n", __FILE__, __LINE__,                     \
                    (a_ != NULL) ? a_ : "(null)", e_);                                                      \
        }                                                                                                   \
    } while (0)

#define RUN(test)                                                                                           \
    do                                                                                                      \
    {                                                                                                       \
        int before_ = g_failed;                                                                             \
        test();                                                                                             \
        printf("%-34s %s\n", #test, (g_failed == before_) ? "ok" : "FAILED");                               \
    } while (0)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    char  *data;
    size_t len;
    size_t cap;
    bool   fail;
} memSink_t;

typedef struct
{
    vcdUartFrame_t frames[64];
    size_t         count;
} frameList_t;

/* ============================================ LOCAL VARIABLES ============================================ */
static int g_checks;
static int g_failed;
static uint64_t g_rng = 0x9E3779B97F4A7C15u;

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static uint32_t rnd(uint32_t range)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t)(g_rng % range);
}

static bool memSinkWrite(void *ctx, const char *data, size_t len)
{
    memSink_t *m = (memSink_t *)ctx;

    if (m->fail)
    {
        return false;
    }
    if ((m->len + len + 1u) > m->cap)
    {
        size_t cap = (m->cap != 0u) ? m->cap : 256u;
        while (cap < (m->len + len + 1u))
        {
            cap *= 2u;
        }
        m->data = (char *)realloc(m->data, cap);
        m->cap = cap;
    }
    memcpy(m->data + m->len, data, len);
    m->len += len;
    m->data[m->len] = '\0';
    return true;
}

static void memSinkFree(memSink_t *m)
{
    free(m->data);
    memset(m, 0, sizeof *m);
}

/* Writer to a memory sink, fixed header so output can be compared literally. */
static vcdWriter_t *openMem(memSink_t *m, vcdOrder_t order, vcdUnit_t unit)
{
    vcdWriterConfig_t cfg;
    vcdWriter_t *w = NULL;

    VCD_WriterConfigDefault(&cfg);
    cfg.date = "";
    cfg.version = "test";
    cfg.order = order;
    cfg.timescale.magnitude = 1u;
    cfg.timescale.unit = unit;
    CHECK_OK(VCD_WriterOpenSink(&w, memSinkWrite, m, &cfg));
    return w;
}

static vcdReader_t *readMem(const memSink_t *m)
{
    vcdReader_t *r = NULL;
    vcdParseError_t err;
    vcdStatus_t st = VCD_ReaderOpenMemory(&r, m->data, m->len, &err);

    CHECK_OK(st);
    if (st != VCD_OK)
    {
        fprintf(stderr, "  parse error line %zu: %s\n", err.line, err.message);
    }
    return r;
}

static size_t countLines(const char *text, const char *prefix)
{
    size_t n = 0u;
    size_t plen = strlen(prefix);
    const char *p = text;

    while (p != NULL && *p != '\0')
    {
        if (strncmp(p, prefix, plen) == 0)
        {
            n++;
        }
        p = strchr(p, '\n');
        if (p != NULL)
        {
            p++;
        }
    }
    return n;
}

static bool collectFrame(void *ctx, const vcdUartFrame_t *frame)
{
    frameList_t *list = (frameList_t *)ctx;
    if (list->count < (sizeof list->frames / sizeof list->frames[0]))
    {
        list->frames[list->count] = *frame;
    }
    list->count++;
    return true;
}

static int compareStr(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* ================================================== TESTS ================================================ */
static void test_ids_unique(void)
{
    enum { N = 20000 };
    static char ids[N][VCD_ID_MAX_LEN];
    static const char *sorted[N];
    size_t i;

    CHECK(vcdEncodeId(0u, ids[0]) == 1u && strcmp(ids[0], "!") == 0);
    CHECK(vcdEncodeId(93u, ids[0]) == 1u && strcmp(ids[0], "~") == 0);
    CHECK(vcdEncodeId(94u, ids[0]) == 2u && strcmp(ids[0], "!!") == 0);
    CHECK(vcdEncodeId(UINT32_MAX, ids[0]) < VCD_ID_MAX_LEN);

    for (i = 0u; i < N; i++)
    {
        (void)vcdEncodeId((uint32_t)i, ids[i]);
        sorted[i] = ids[i];
        for (const char *p = ids[i]; *p != '\0'; p++)
        {
            CHECK(*p >= '!' && *p <= '~');
        }
    }
    qsort(sorted, N, sizeof sorted[0], compareStr);
    for (i = 1u; i < N; i++)
    {
        CHECK(strcmp(sorted[i - 1u], sorted[i]) != 0);
    }
}

static void test_time_helpers(void)
{
    vcdTimescale_t us = {1u, VCD_UNIT_US};
    vcdTimescale_t ns10 = {10u, VCD_UNIT_NS};
    vcdTimescale_t bad = {3u, VCD_UNIT_NS};
    uint64_t v = 0u;

    CHECK(VCD_TimescaleValid(us));
    CHECK(!VCD_TimescaleValid(bad));
    CHECK(fabs(VCD_TimescaleSeconds(ns10) - 1e-8) < 1e-20);
    CHECK(VCD_SecondsToTicks(us, 0.0012345) == 1235u);
    CHECK(VCD_SecondsToTicks(us, -1.0) == 0u);
    CHECK(fabs(VCD_TicksToSeconds(us, 1000u) - 1e-3) < 1e-15);
    CHECK(fabs(VCD_UartBitTicks(us, 10400.0) - 96.153846) < 1e-5);

    CHECK_OK(VCD_ValueToU64("b1010", &v));
    CHECK(v == 10u);
    CHECK_OK(VCD_ValueToU64("0000000000000000000000000000000000000000000000000000000000000000000001", &v));
    CHECK(v == 1u);
    CHECK_ST(VCD_ValueToU64("1x", &v), VCD_ERR_VALUE);
    CHECK_ST(VCD_ValueToU64("", &v), VCD_ERR_VALUE);
    CHECK_ST(VCD_ValueToU64("10000000000000000000000000000000000000000000000000000000000000000", &v),
             VCD_ERR_VALUE);
}

static void test_golden_output(void)
{
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_STRICT, VCD_UNIT_US);
    vcdSignal_t a;
    vcdSignal_t bus;
    vcdSignal_t state;
    vcdSignal_t level;

    CHECK_OK(VCD_ScopeBegin(w, "top"));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "a", &a));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 8u, "bus[7:0]", &bus));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_STRING, 0u, "state", &state));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_REAL, 0u, "level", &level));
    CHECK_OK(VCD_ScopeEnd(w));

    CHECK_OK(VCD_ChangeBit(w, a, 0u, '1'));
    CHECK_OK(VCD_ChangeString(w, state, 0u, "IDLE"));
    CHECK_OK(VCD_ChangeU64(w, bus, 10u, 0x41u));
    CHECK_OK(VCD_ChangeBit(w, a, 10u, '0'));
    CHECK_OK(VCD_ChangeBit(w, a, 20u, '0')); /* redundant: no #20 at all */
    CHECK_OK(VCD_ChangeString(w, state, 30u, "RUN NOW"));
    CHECK_OK(VCD_ChangeReal(w, level, 30u, 2.5));
    CHECK_OK(VCD_WriterClose(w, 40u));

    CHECK_STR(m.data,
              "$version\n\ttest\n$end\n"
              "$timescale 1us $end\n"
              "$scope module top $end\n"
              "$var wire 1 ! a $end\n"
              "$var wire 8 \" bus[7:0] $end\n"
              "$var string 1 # state $end\n"
              "$var real 64 $ level $end\n"
              "$upscope $end\n"
              "$enddefinitions $end\n"
              "#0\n"
              "$dumpvars\n"
              "1!\n"
              "bx \"\n"
              "sIDLE #\n"
              "$end\n"
              "#10\n"
              "b1000001 \"\n"
              "0!\n"
              "#30\n"
              "sRUN_NOW #\n"
              "r2.5 $\n"
              "#40\n");
    memSinkFree(&m);
}

static void test_strict_order(void)
{
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_STRICT, VCD_UNIT_NS);
    vcdSignal_t a;

    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "a", &a));
    CHECK_OK(VCD_ChangeBit(w, a, 10u, '1'));
    CHECK_ST(VCD_ChangeBit(w, a, 5u, '0'), VCD_ERR_TIME);
    CHECK_OK(VCD_WriterStatus(w)); /* not sticky */
    CHECK_OK(VCD_ChangeBit(w, a, 10u, '0'));
    CHECK_OK(VCD_ChangeBit(w, a, 12u, '1'));
    CHECK_ST(VCD_ChangeBit(w, a, 11u, '1'), VCD_ERR_TIME);
    CHECK_ST(VCD_ChangeBit(w, a, VCD_TIME_NONE, '1'), VCD_ERR_ARG);
    CHECK_ST(VCD_WriterClose(w, 5u), VCD_ERR_TIME); /* end before last change: reported, file complete */
    CHECK(strstr(m.data, "#10\n$dumpvars\n0!\n$end\n#12\n1!\n") != NULL);
    memSinkFree(&m);
}

static void test_same_time_collapse(void)
{
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_STRICT, VCD_UNIT_NS);
    vcdSignal_t a;
    vcdSignal_t b;

    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "a", &a));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "b", &b));
    CHECK_OK(VCD_ChangeBit(w, a, 0u, '0'));
    CHECK_OK(VCD_ChangeBit(w, b, 0u, '0'));
    CHECK_OK(VCD_ChangeBit(w, a, 5u, '1'));
    CHECK_OK(VCD_ChangeBit(w, a, 5u, '0'));
    CHECK_OK(VCD_ChangeBit(w, a, 5u, '1'));
    CHECK_OK(VCD_ChangeBit(w, b, 7u, '1'));
    CHECK_OK(VCD_ChangeBit(w, b, 7u, '0')); /* back to its old value: nothing at #7 */
    CHECK_OK(VCD_WriterClose(w, VCD_TIME_NONE));
    CHECK(strstr(m.data, "$end\n#5\n1!\n") != NULL);
    CHECK(countLines(m.data, "#") == 2u);
    CHECK(strstr(m.data, "#7") == NULL);
    memSinkFree(&m);
}

static void test_redundant_kept_when_disabled(void)
{
    memSink_t m = {0};
    vcdWriterConfig_t cfg;
    vcdWriter_t *w = NULL;
    vcdSignal_t a;

    VCD_WriterConfigDefault(&cfg);
    cfg.date = "";
    cfg.dropRedundant = false;
    CHECK_OK(VCD_WriterOpenSink(&w, memSinkWrite, &m, &cfg));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "a", &a));
    CHECK_OK(VCD_ChangeBit(w, a, 0u, '1'));
    CHECK_OK(VCD_ChangeBit(w, a, 1u, '1'));
    CHECK_OK(VCD_ChangeBit(w, a, 2u, '1'));
    CHECK_OK(VCD_WriterClose(w, VCD_TIME_NONE));
    CHECK(countLines(m.data, "1!") == 3u);
    memSinkFree(&m);
}

static void test_sorted_mode(void)
{
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_SORTED, VCD_UNIT_NS);
    vcdReader_t *r;
    vcdSignal_t a;
    vcdSignal_t b;
    size_t va;
    size_t vb;
    vcdTime_t t;
    const char *v;

    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "a", &a));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 4u, "b", &b));
    CHECK_OK(VCD_ChangeBit(w, a, 100u, '1'));
    CHECK_OK(VCD_ChangeBit(w, a, 50u, '0'));
    CHECK_OK(VCD_ChangeU64(w, b, 75u, 9u));
    CHECK_OK(VCD_ChangeU64(w, b, 60u, 3u));
    CHECK_OK(VCD_Commit(w, 80u));
    CHECK_ST(VCD_ChangeBit(w, a, 79u, '1'), VCD_ERR_TIME);
    CHECK_OK(VCD_ChangeBit(w, a, 80u, 'z'));
    CHECK_OK(VCD_WriterClose(w, 200u));

    r = readMem(&m);
    if (r == NULL)
    {
        memSinkFree(&m);
        return;
    }
    CHECK_OK(VCD_ReaderFind(r, "a", &va));
    CHECK_OK(VCD_ReaderFind(r, "b", &vb));
    CHECK(VCD_ReaderChangeCount(r, va) == 3u);
    CHECK_OK(VCD_ReaderChange(r, va, 0u, &t, &v));
    CHECK(t == 50u && strcmp(v, "0") == 0);
    CHECK_OK(VCD_ReaderChange(r, va, 1u, &t, &v));
    CHECK(t == 80u && strcmp(v, "z") == 0);
    CHECK_OK(VCD_ReaderChange(r, va, 2u, &t, &v));
    CHECK(t == 100u && strcmp(v, "1") == 0);
    CHECK_STR(VCD_ReaderValueAt(r, vb, 50u), "x"); /* from $dumpvars */
    CHECK_STR(VCD_ReaderValueAt(r, vb, 70u), "11");
    CHECK_STR(VCD_ReaderValueAt(r, vb, 1000u), "1001");
    CHECK(VCD_ReaderEndTime(r) == 200u);
    VCD_ReaderClose(r);
    memSinkFree(&m);
}

static void test_values_and_kinds(void)
{
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_STRICT, VCD_UNIT_NS);
    vcdReader_t *r;
    vcdSignal_t bit;
    vcdSignal_t bus;
    vcdSignal_t wide;
    vcdSignal_t num;
    vcdSignal_t real;
    vcdSignal_t text;
    char longBits[201];
    size_t idx;
    uint64_t v;

    CHECK_ST(VCD_AddVar(w, VCD_VAR_WIRE, 0u, "zero", NULL), VCD_ERR_ARG);
    CHECK_ST(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "has space", NULL), VCD_ERR_ARG);
    CHECK_ST(VCD_AddVar(w, VCD_VAR_EVENT, 1u, "ev", NULL), VCD_ERR_ARG);
    CHECK_ST(VCD_ScopeEnd(w), VCD_ERR_STATE);
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "bit", &bit));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_REG, 8u, "bus", &bus));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 200u, "wide", &wide));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_INTEGER, 0u, "num", &num));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_REAL, 0u, "real", &real));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_STRING, 0u, "text", &text));

    CHECK_ST(VCD_ChangeBit(w, bus, 0u, '1'), VCD_ERR_ARG);
    CHECK_ST(VCD_ChangeBit(w, bit, 0u, '2'), VCD_ERR_VALUE);
    CHECK_ST(VCD_ChangeU64(w, bus, 0u, 0x100u), VCD_ERR_VALUE);
    CHECK_ST(VCD_ChangeBits(w, bus, 0u, "101010101"), VCD_ERR_VALUE);
    CHECK_ST(VCD_ChangeBits(w, bus, 0u, "10a"), VCD_ERR_VALUE);
    CHECK_ST(VCD_ChangeReal(w, bus, 0u, 1.0), VCD_ERR_ARG);
    CHECK_ST(VCD_ChangeReal(w, real, 0u, NAN), VCD_ERR_VALUE);
    CHECK_ST(VCD_ChangeString(w, bus, 0u, "x"), VCD_ERR_ARG);
    CHECK_ST(VCD_ChangeU64(w, 99, 0u, 1u), VCD_ERR_ARG);

    CHECK_OK(VCD_ChangeBit(w, bit, 0u, 'X'));
    CHECK_OK(VCD_ChangeBits(w, bus, 1u, "0001x"));
    CHECK_OK(VCD_ChangeBits(w, bus, 2u, "xx01"));
    CHECK_OK(VCD_ChangeBits(w, bus, 3u, "ZZZZ"));
    CHECK_OK(VCD_ChangeBits(w, bus, 4u, "0000"));
    memset(longBits, '1', 200u);
    longBits[200] = '\0';
    CHECK_OK(VCD_ChangeBits(w, wide, 5u, longBits));
    CHECK_OK(VCD_ChangeU64(w, num, 6u, 0xDEADBEEFu));
    CHECK_OK(VCD_ChangeReal(w, real, 7u, -1.0e-9));
    CHECK_OK(VCD_ChangeString(w, text, 8u, "hello world\tX"));
    CHECK_OK(VCD_ChangeBool(w, bit, 9u, true));
    CHECK_ST(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "late", NULL), VCD_ERR_STATE);
    CHECK_ST(VCD_ScopeBegin(w, "late"), VCD_ERR_STATE);
    CHECK_OK(VCD_WriterClose(w, 10u));

    r = readMem(&m);
    if (r == NULL)
    {
        memSinkFree(&m);
        return;
    }
    CHECK_OK(VCD_ReaderFind(r, "bit", &idx));
    CHECK_STR(VCD_ReaderValueAt(r, idx, 0u), "x");
    CHECK_STR(VCD_ReaderValueAt(r, idx, 9u), "1");
    CHECK_OK(VCD_ReaderFind(r, "bus", &idx));
    CHECK_STR(VCD_ReaderValueAt(r, idx, 1u), "1x");
    CHECK_STR(VCD_ReaderValueAt(r, idx, 2u), "x01");
    CHECK_STR(VCD_ReaderValueAt(r, idx, 3u), "z");
    CHECK_STR(VCD_ReaderValueAt(r, idx, 4u), "0");
    CHECK_OK(VCD_ReaderFind(r, "wide", &idx));
    CHECK_STR(VCD_ReaderValueAt(r, idx, 5u), longBits);
    CHECK_OK(VCD_ReaderFind(r, "num", &idx));
    CHECK(VCD_ReaderVar(r, idx)->width == 32u);
    CHECK(VCD_ReaderVar(r, idx)->type == VCD_VAR_INTEGER);
    CHECK_OK(VCD_ValueToU64(VCD_ReaderValueAt(r, idx, 6u), &v));
    CHECK(v == 0xDEADBEEFu);
    CHECK_OK(VCD_ReaderFind(r, "real", &idx));
    CHECK(VCD_ReaderVar(r, idx)->type == VCD_VAR_REAL);
    CHECK(strtod(VCD_ReaderValueAt(r, idx, 7u), NULL) == -1.0e-9);
    CHECK_OK(VCD_ReaderFind(r, "text", &idx));
    CHECK_STR(VCD_ReaderValueAt(r, idx, 8u), "hello_world_X");
    VCD_ReaderClose(r);
    memSinkFree(&m);
}

static void test_header_options(void)
{
    memSink_t m = {0};
    vcdWriterConfig_t cfg;
    vcdWriter_t *w = NULL;
    vcdReader_t *r;

    VCD_WriterConfigDefault(&cfg);
    cfg.comment = "made by a test";
    cfg.timescale.magnitude = 100u;
    cfg.timescale.unit = VCD_UNIT_PS;
    CHECK_OK(VCD_WriterOpenSink(&w, memSinkWrite, &m, &cfg));
    CHECK_OK(VCD_WriterClose(w, VCD_TIME_NONE));

    r = readMem(&m);
    if (r != NULL)
    {
        CHECK(VCD_ReaderDate(r) != NULL); /* automatic date */
        CHECK_STR(VCD_ReaderVersion(r), "VCDio " VCDIO_VERSION_STRING);
        CHECK_STR(VCD_ReaderComment(r), "made by a test");
        CHECK(VCD_ReaderTimescale(r).magnitude == 100u && VCD_ReaderTimescale(r).unit == VCD_UNIT_PS);
        CHECK(VCD_ReaderVarCount(r) == 0u);
        VCD_ReaderClose(r);
    }
    memSinkFree(&m);

    cfg.comment = "bad $end inside";
    CHECK_ST(VCD_WriterOpenSink(&w, memSinkWrite, &m, &cfg), VCD_ERR_ARG);
    cfg.comment = NULL;
    cfg.timescale.magnitude = 5u;
    CHECK_ST(VCD_WriterOpenSink(&w, memSinkWrite, &m, &cfg), VCD_ERR_ARG);
    CHECK_ST(VCD_WriterOpenSink(&w, NULL, &m, NULL), VCD_ERR_ARG);
}

static void test_sink_failure(void)
{
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_STRICT, VCD_UNIT_NS);
    vcdSignal_t a;
    vcdTime_t t;

    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "a", &a));
    m.fail = true;
    /* enough output to overflow the internal buffer and hit the sink */
    for (t = 0u; t < 5000u; t++)
    {
        if (VCD_ChangeBit(w, a, t, ((t & 1u) != 0u) ? '1' : '0') != VCD_OK)
        {
            break;
        }
    }
    CHECK(t < 5000u);
    CHECK_ST(VCD_WriterStatus(w), VCD_ERR_IO);
    CHECK_ST(VCD_ChangeBit(w, a, t + 1u, '1'), VCD_ERR_IO); /* sticky */
    CHECK_ST(VCD_WriterClose(w, VCD_TIME_NONE), VCD_ERR_IO);
    memSinkFree(&m);
}

static void test_file_round_trip(void)
{
    const char *path = "vcdio_test_round_trip.vcd";
    vcdWriter_t *w = NULL;
    vcdReader_t *r = NULL;
    vcdParseError_t err;
    vcdSignal_t a;
    size_t idx;

    CHECK_OK(VCD_WriterOpenFile(&w, path, NULL));
    if (w == NULL)
    {
        return;
    }
    CHECK_OK(VCD_ScopeBegin(w, "dut"));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "clk", &a));
    for (vcdTime_t t = 0u; t < 100u; t += 5u)
    {
        CHECK_OK(VCD_ChangeBool(w, a, t, ((t / 5u) & 1u) != 0u));
    }
    CHECK_OK(VCD_Flush(w));
    CHECK_OK(VCD_WriterClose(w, 100u));

    CHECK_OK(VCD_ReaderOpenFile(&r, path, &err));
    if (r != NULL)
    {
        CHECK_OK(VCD_ReaderFind(r, "dut.clk", &idx));
        CHECK(VCD_ReaderChangeCount(r, idx) == 20u);
        CHECK_STR(VCD_ReaderValueAt(r, idx, 7u), "1");
        CHECK(VCD_ReaderValueAt(r, idx, 0u) != NULL);
        VCD_ReaderClose(r);
    }
    (void)remove(path);

    CHECK_ST(VCD_ReaderOpenFile(&r, "does/not/exist.vcd", &err), VCD_ERR_IO);
    CHECK(r == NULL);
    CHECK(strstr(err.message, "cannot read") != NULL);
    CHECK_ST(VCD_WriterOpenFile(&w, "does/not/exist.vcd", NULL), VCD_ERR_IO);
}

static void test_reader_foreign_file(void)
{
    static const char text[] =
        "$date\n\tMon Jan 22 22:32:32 2024\n$end\n"
        "$version\n\tIcarus Verilog\n$end\n"
        "$timescale\n\t10ps\n$end\n"
        "$scope module tb $end\n"
        "$var wire 8 ! value [7:0] $end\n"
        "$var reg 1 \" clk $end\n"
        "$var logic 1 %% rstn $end\n"
        "$scope module u0 $end\n"
        "$var wire 1 \" clk $end\n"
        "$var real 1 # temp $end\n"
        "$var string 1 $ msg $end\n"
        "$var wire 1 aB2 long_id $end\n"
        "$upscope $end\n"
        "$upscope $end\n"
        "$comment unknown sections are skipped $end\n"
        "$enddefinitions $end\n"
        "#0\n"
        "$dumpvars\n"
        "b0 !\n"
        "1\"\n"
        "U%%\n"
        "r1.5 #\n"
        "sHELLO $\n"
        "XaB2\n"
        "$end\n"
        "#5\n"
        "0\"\n"
        "B1010 !\n"
        "1aB2\n"
        "#10\n"
        "1\"\n"
        "R-2 #\n"
        "$dumpoff\n"
        "x\"\n"
        "$end\n"
        "#15\n";
    vcdReader_t *r = NULL;
    vcdParseError_t err;
    size_t value;
    size_t clk;
    size_t clk2;
    size_t idx;
    vcdTime_t t;

    CHECK_OK(VCD_ReaderOpenMemory(&r, text, strlen(text), &err));
    if (r == NULL)
    {
        fprintf(stderr, "  line %zu: %s\n", err.line, err.message);
        return;
    }
    CHECK_STR(VCD_ReaderDate(r), "Mon Jan 22 22:32:32 2024");
    CHECK_STR(VCD_ReaderVersion(r), "Icarus Verilog");
    CHECK(VCD_ReaderTimescale(r).magnitude == 10u && VCD_ReaderTimescale(r).unit == VCD_UNIT_PS);
    CHECK(VCD_ReaderVarCount(r) == 7u);
    CHECK(VCD_ReaderStartTime(r) == 0u && VCD_ReaderEndTime(r) == 15u);

    CHECK_OK(VCD_ReaderFind(r, "tb.value[7:0]", &value));
    CHECK_OK(VCD_ReaderFind(r, "value", &idx)); /* leaf name without range */
    CHECK(idx == value);
    CHECK_STR(VCD_ReaderVar(r, value)->scope, "tb");
    CHECK(VCD_ReaderVar(r, value)->width == 8u);
    CHECK_STR(VCD_ReaderValueAt(r, value, 7u), "1010");

    CHECK_OK(VCD_ReaderFind(r, "tb.clk", &clk));
    CHECK_OK(VCD_ReaderFind(r, "tb.u0.clk", &clk2));
    CHECK(clk != clk2);
    CHECK(VCD_ReaderChangeCount(r, clk) == 4u && VCD_ReaderChangeCount(r, clk2) == 4u);
    CHECK_OK(VCD_ReaderFind(r, "clk", &idx)); /* aliases of one signal: not ambiguous */
    CHECK_STR(VCD_ReaderValueAt(r, clk2, 12u), "x");
    CHECK_OK(VCD_ReaderChange(r, clk, 3u, &t, NULL));
    CHECK(t == 10u);

    CHECK_OK(VCD_ReaderFind(r, "rstn", &idx));
    CHECK_STR(VCD_ReaderVar(r, idx)->typeName, "logic");
    CHECK(VCD_ReaderVar(r, idx)->type == VCD_VAR_REG);
    CHECK_STR(VCD_ReaderValueAt(r, idx, 0u), "u");

    CHECK_OK(VCD_ReaderFind(r, "temp", &idx));
    CHECK_STR(VCD_ReaderValueAt(r, idx, 3u), "1.5");
    CHECK_STR(VCD_ReaderValueAt(r, idx, 10u), "-2");
    CHECK_OK(VCD_ReaderFind(r, "msg", &idx));
    CHECK_STR(VCD_ReaderValueAt(r, idx, 0u), "HELLO");
    CHECK_OK(VCD_ReaderFind(r, "long_id", &idx));
    CHECK_STR(VCD_ReaderVar(r, idx)->id, "aB2");
    CHECK_STR(VCD_ReaderValueAt(r, idx, 0u), "x");
    CHECK_STR(VCD_ReaderValueAt(r, idx, 5u), "1");

    CHECK_ST(VCD_ReaderFindChange(r, value, 0u, &idx), VCD_OK);
    CHECK_ST(VCD_ReaderFind(r, "nope", &idx), VCD_ERR_NOT_FOUND);
    CHECK(VCD_ReaderVar(r, 99u) == NULL);
    CHECK(VCD_ReaderValueAt(r, 99u, 0u) == NULL);
    VCD_ReaderClose(r);
}

static void test_reader_ambiguous_and_empty(void)
{
    static const char text[] =
        "$scope module a $end $var wire 1 ! clk $end $upscope $end\n"
        "$scope module b $end $var wire 1 \" clk $end $upscope $end\n"
        "$enddefinitions $end\n"
        "#3\n0!\n";
    vcdReader_t *r = NULL;
    size_t idx;

    CHECK_OK(VCD_ReaderOpenMemory(&r, text, strlen(text), NULL));
    if (r != NULL)
    {
        CHECK_ST(VCD_ReaderFind(r, "clk", &idx), VCD_ERR_ARG);
        CHECK_OK(VCD_ReaderFind(r, "b.clk", &idx));
        CHECK(VCD_ReaderValueAt(r, idx, 100u) == NULL); /* never changed */
        CHECK_OK(VCD_ReaderFind(r, "a.clk", &idx));
        CHECK(VCD_ReaderValueAt(r, idx, 2u) == NULL);   /* before first change */
        CHECK_STR(VCD_ReaderValueAt(r, idx, 3u), "0");
        CHECK(VCD_ReaderTimescale(r).unit == VCD_UNIT_NS); /* default */
        VCD_ReaderClose(r);
    }

    CHECK_OK(VCD_ReaderOpenMemory(&r, "", 0u, NULL));
    CHECK(r != NULL && VCD_ReaderVarCount(r) == 0u);
    VCD_ReaderClose(r);
}

static void test_reader_errors(void)
{
    static const struct
    {
        const char *text;
        size_t      line;
        const char *needle;
    } cases[] = {
        {"$var wire 1 ! a $end\n$enddefinitions $end\n#abc\n", 3u, "invalid time"},
        {"$var wire 1 ! a $end\n$enddefinitions $end\n#0\n1?\n", 4u, "unknown identifier"},
        {"$var wire 1 ! a $end\n#10\n1!\n#5\n", 4u, "backwards"},
        {"$date today", 1u, "missing $end"},
        {"$var wire 1 ! $end", 1u, "$var needs"},
        {"$var wire 0 ! a $end", 1u, "width"},
        {"$timescale 3 parsecs $end", 1u, "timescale"},
        {"?hello", 1u, "unexpected"},
        {"hello", 1u, "unknown identifier"}, /* 'h' is a VHDL level, so this is value h on id "ello" */
        {"$var wire 1 ! a $end\nb !\n", 2u, "missing value"},
        {"$var wire 1 ! a $end\nb101", 2u, "missing identifier"},
    };
    size_t i;

    for (i = 0u; i < (sizeof cases / sizeof cases[0]); i++)
    {
        vcdReader_t *r = NULL;
        vcdParseError_t err;
        vcdStatus_t st = VCD_ReaderOpenMemory(&r, cases[i].text, strlen(cases[i].text), &err);

        CHECK_ST(st, VCD_ERR_PARSE);
        CHECK(r == NULL);
        CHECK(err.line == cases[i].line);
        CHECK(strstr(err.message, cases[i].needle) != NULL);
        if ((err.line != cases[i].line) || (strstr(err.message, cases[i].needle) == NULL))
        {
            fprintf(stderr, "  case %zu: line %zu: %s\n", i, err.line, err.message);
        }
    }
}

/* 'A' at 10400 baud with an exact 96 tick bit: the edges from the VCD tutorial. */
static void test_uart_frame_edges(void)
{
    static const vcdTime_t times[] = {0u, 100u, 196u, 292u, 772u, 868u, 964u};
    static const char *const values[] = {"1", "0", "1", "0", "1", "0", "1"};
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_STRICT, VCD_UNIT_US);
    vcdUartConfig_t cfg;
    vcdReader_t *r;
    vcdSignal_t tx;
    vcdTime_t end = 0u;
    size_t idx;
    size_t i;

    VCD_UartConfigDefault(&cfg, VCD_WriterTimescale(w), 10400.0);
    cfg.bitTicks = 96.0;
    CHECK(VCD_UartFrameTicks(&cfg) == 960u);
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "tx", &tx));
    CHECK_OK(VCD_UartWriteIdle(w, tx, 0u, &cfg));
    CHECK_OK(VCD_UartWriteFrame(w, tx, 100u, 0x41u, &cfg, 0u, &end));
    CHECK(end == 1060u);
    CHECK_ST(VCD_UartWriteFrame(w, tx, 2000u, 0x100u, &cfg, 0u, NULL), VCD_ERR_VALUE);
    CHECK_OK(VCD_WriterClose(w, 1100u));

    r = readMem(&m);
    if (r == NULL)
    {
        memSinkFree(&m);
        return;
    }
    CHECK_OK(VCD_ReaderFind(r, "tx", &idx));
    CHECK(VCD_ReaderChangeCount(r, idx) == 7u);
    for (i = 0u; i < 7u; i++)
    {
        vcdTime_t t = 0u;
        const char *v = NULL;
        CHECK_OK(VCD_ReaderChange(r, idx, i, &t, &v));
        CHECK(t == times[i]);
        CHECK_STR(v, values[i]);
    }
    VCD_ReaderClose(r);
    memSinkFree(&m);
}

static void test_uart_data_signal_and_break(void)
{
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_STRICT, VCD_UNIT_US);
    vcdUartConfig_t cfg;
    vcdReader_t *r;
    frameList_t list = {0};
    vcdSignal_t tx;
    vcdSignal_t data;
    static const uint8_t bytes[] = {0xC1u, 0x33u, 0xF1u};
    vcdTime_t end = 0u;
    size_t line;
    size_t dataVar;
    uint64_t v;

    VCD_UartConfigDefault(&cfg, VCD_WriterTimescale(w), 10400.0);
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "tx", &tx));
    CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 8u, "tx_data", &data));
    cfg.dataSignal = data;
    CHECK_OK(VCD_UartWriteIdle(w, tx, 0u, &cfg));
    CHECK_OK(VCD_UartWriteBytes(w, tx, 1000u, bytes, sizeof bytes, 500u, &cfg, &end));
    CHECK_OK(VCD_UartWriteBreak(w, tx, end + 1000u, 5000u, &cfg));
    CHECK_OK(VCD_WriterClose(w, end + 10000u));

    r = readMem(&m);
    if (r == NULL)
    {
        memSinkFree(&m);
        return;
    }
    CHECK_OK(VCD_ReaderFind(r, "tx", &line));
    CHECK_OK(VCD_ReaderFind(r, "tx_data", &dataVar));
    CHECK_OK(VCD_ValueToU64(VCD_ReaderValueAt(r, dataVar, 1500u), &v));
    CHECK(v == 0xC1u);
    CHECK_STR(VCD_ReaderValueAt(r, dataVar, 2200u), "z");

    VCD_UartConfigDefault(&cfg, VCD_ReaderTimescale(r), 10400.0);
    CHECK_OK(VCD_UartDecode(r, line, &cfg, collectFrame, &list));
    CHECK(list.count == 4u); /* 3 bytes + the break, read as 0x00 with a framing error */
    if (list.count == 4u)
    {
        CHECK(list.frames[0].data == 0xC1u && list.frames[0].errors == 0u && list.frames[0].start == 1000u);
        CHECK(list.frames[1].data == 0x33u && list.frames[1].errors == 0u);
        CHECK(list.frames[2].data == 0xF1u && list.frames[2].errors == 0u);
        CHECK(list.frames[2].end == end);
        CHECK(list.frames[3].data == 0x00u && list.frames[3].errors == VCD_UART_ERR_FRAMING);
    }
    CHECK_ST(VCD_UartDecode(r, dataVar, &cfg, collectFrame, &list), VCD_ERR_ARG); /* not 1 bit */
    VCD_ReaderClose(r);
    memSinkFree(&m);
}

/* Random UART configurations and traffic: write, read back, decode, compare. */
static void test_uart_round_trip_random(void)
{
    static const double bauds[] = {5.0, 300.0, 9600.0, 10400.0, 115200.0, 1000000.0};
    static const vcdUnit_t units[] = {VCD_UNIT_US, VCD_UNIT_NS, VCD_UNIT_PS};
    static const uint16_t mags[] = {1u, 10u, 100u};
    unsigned iter;
    unsigned runs = 0u;

    for (iter = 0u; iter < 400u; iter++)
    {
        memSink_t m = {0};
        vcdWriterConfig_t wcfg;
        vcdUartConfig_t cfg;
        vcdWriter_t *w = NULL;
        vcdReader_t *r = NULL;
        vcdSignal_t line;
        vcdSignal_t data;
        frameList_t list = {0};
        vcdUartFrame_t expect[24];
        size_t count = 1u + rnd(24u);
        vcdTime_t t;
        size_t var;
        size_t i;
        bool sorted = rnd(2u) != 0u;

        VCD_WriterConfigDefault(&wcfg);
        wcfg.date = "";
        wcfg.order = sorted ? VCD_ORDER_SORTED : VCD_ORDER_STRICT;
        wcfg.timescale.unit = units[rnd(3u)];
        wcfg.timescale.magnitude = mags[rnd(3u)];
        wcfg.dropRedundant = rnd(4u) != 0u;

        VCD_UartConfigDefault(&cfg, wcfg.timescale, bauds[rnd(6u)]);
        if ((cfg.bitTicks < 8.0) || (cfg.bitTicks > 1e9))
        {
            continue;
        }
        cfg.dataBits = (uint8_t)(5u + rnd(5u));
        cfg.parity = (vcdUartParity_t)rnd(5u);
        cfg.stopBits = (vcdUartStop_t)rnd(3u);
        cfg.msbFirst = rnd(2u) != 0u;
        cfg.inverted = rnd(2u) != 0u;
        runs++;

        CHECK_OK(VCD_WriterOpenSink(&w, memSinkWrite, &m, &wcfg));
        CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 1u, "line", &line));
        CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 9u, "data", &data));
        cfg.dataSignal = sorted ? VCD_SIGNAL_NONE : data;

        /* schedule */
        t = (vcdTime_t)(2.0 * cfg.bitTicks) + rnd(1000u);
        for (i = 0u; i < count; i++)
        {
            uint32_t flags = 0u;
            if ((cfg.parity != VCD_UART_PARITY_NONE) && (rnd(5u) == 0u))
            {
                flags |= VCD_UART_FLAG_PARITY_ERROR;
            }
            if (rnd(7u) == 0u)
            {
                flags |= VCD_UART_FLAG_FRAMING_ERROR;
            }
            expect[i].start = t;
            expect[i].data = (uint16_t)rnd(1u << cfg.dataBits);
            expect[i].errors = ((flags & VCD_UART_FLAG_PARITY_ERROR) ? VCD_UART_ERR_PARITY : 0u) |
                               ((flags & VCD_UART_FLAG_FRAMING_ERROR) ? VCD_UART_ERR_FRAMING : 0u);
            expect[i].end = flags; /* reuse to remember the flags while writing */
            t += VCD_UartFrameTicks(&cfg);
            /* after a missing stop bit the line needs idle time, or no new start edge exists */
            t += (vcdTime_t)(cfg.bitTicks * (double)(rnd(4u) + ((flags & VCD_UART_FLAG_FRAMING_ERROR) ? 1u : 0u)));
        }

        CHECK_OK(VCD_UartWriteIdle(w, line, 0u, &cfg));
        for (i = 0u; i < count; i++)
        {
            /* sorted mode: write the frames newest first to exercise the sorting */
            size_t k = sorted ? (count - 1u - i) : i;
            vcdTime_t end = 0u;
            CHECK_OK(VCD_UartWriteFrame(w, line, expect[k].start, expect[k].data, &cfg, (uint32_t)expect[k].end,
                                        &end));
            expect[k].end = end;
        }
        CHECK_OK(VCD_WriterClose(w, t + (vcdTime_t)(10.0 * cfg.bitTicks)));

        r = readMem(&m);
        if (r != NULL)
        {
            vcdUartConfig_t dcfg = cfg;
            CHECK_OK(VCD_ReaderFind(r, "line", &var));
            CHECK_OK(VCD_UartDecode(r, var, &dcfg, collectFrame, &list));
            CHECK(list.count == count);
            for (i = 0u; (i < count) && (i < list.count); i++)
            {
                bool same = (list.frames[i].start == expect[i].start) && (list.frames[i].end == expect[i].end) &&
                            (list.frames[i].data == expect[i].data) && (list.frames[i].errors == expect[i].errors);
                CHECK(same);
                if (!same)
                {
                    fprintf(stderr,
                            "  iter %u frame %zu: got %04X err %u @%llu, want %04X err %u @%llu "
                            "(bits %u parity %d stop %d msb %d inv %d ticks %.3f)\n",
                            iter, i, list.frames[i].data, (unsigned)list.frames[i].errors,
                            (unsigned long long)list.frames[i].start, expect[i].data, (unsigned)expect[i].errors,
                            (unsigned long long)expect[i].start, cfg.dataBits, (int)cfg.parity, (int)cfg.stopBits,
                            (int)cfg.msbFirst, (int)cfg.inverted, cfg.bitTicks);
                    break;
                }
                if (!sorted)
                {
                    uint64_t shown = 0u;
                    vcdTime_t mid = (expect[i].start + expect[i].end) / 2u;
                    size_t dv;
                    CHECK_OK(VCD_ReaderFind(r, "data", &dv));
                    CHECK_OK(VCD_ValueToU64(VCD_ReaderValueAt(r, dv, mid), &shown));
                    CHECK(shown == expect[i].data);
                }
            }
            VCD_ReaderClose(r);
        }
        memSinkFree(&m);
    }
    CHECK(runs > 100u);
}

/* Many random-order changes with periodic commits, checked against a reference model. */
typedef struct
{
    vcdTime_t time;
    uint64_t  seq;
    uint32_t  sig;
    uint32_t  value;
} refChange_t;

static int compareRef(const void *a, const void *b)
{
    const refChange_t *x = (const refChange_t *)a;
    const refChange_t *y = (const refChange_t *)b;
    if (x->sig != y->sig)
    {
        return (x->sig < y->sig) ? -1 : 1;
    }
    if (x->time != y->time)
    {
        return (x->time < y->time) ? -1 : 1;
    }
    return (x->seq < y->seq) ? -1 : ((x->seq > y->seq) ? 1 : 0);
}

static void test_sorted_stress(void)
{
    enum { SIGS = 4, CHANGES = 60000 };
    static refChange_t ref[CHANGES];
    memSink_t m = {0};
    vcdWriter_t *w = openMem(&m, VCD_ORDER_SORTED, VCD_UNIT_NS);
    vcdSignal_t sig[SIGS];
    vcdReader_t *r;
    vcdTime_t watermark = 0u;
    size_t n = 0u;
    size_t i;
    char name[8];

    for (i = 0u; i < SIGS; i++)
    {
        (void)snprintf(name, sizeof name, "s%zu", i);
        CHECK_OK(VCD_AddVar(w, VCD_VAR_WIRE, 16u, name, &sig[i]));
    }
    for (i = 0u; i < CHANGES; i++)
    {
        refChange_t c;
        c.time = watermark + rnd(5000u);
        c.seq = i;
        c.sig = rnd(SIGS);
        c.value = rnd(4u); /* few values -> many redundant changes */
        CHECK_OK(VCD_ChangeU64(w, sig[c.sig], c.time, c.value));
        ref[n++] = c;
        if ((i % 1000u) == 999u)
        {
            watermark += 2500u;
            CHECK_OK(VCD_Commit(w, watermark));
        }
    }
    CHECK_OK(VCD_WriterClose(w, VCD_TIME_NONE));

    r = readMem(&m); /* also proves the time stamps are monotonic */
    if (r == NULL)
    {
        memSinkFree(&m);
        return;
    }
    qsort(ref, n, sizeof ref[0], compareRef);
    for (i = 0u; i < 3000u; i++)
    {
        uint32_t s = rnd(SIGS);
        vcdTime_t probe = rnd((uint32_t)(watermark + 5000u));
        const refChange_t *last = NULL;
        const char *got;
        size_t var;
        uint64_t v = 0u;

        for (size_t k = 0u; k < n; k++)
        {
            if ((ref[k].sig == s) && (ref[k].time <= probe))
            {
                last = &ref[k];
            }
        }
        (void)snprintf(name, sizeof name, "s%u", (unsigned)s);
        CHECK_OK(VCD_ReaderFind(r, name, &var));
        got = VCD_ReaderValueAt(r, var, probe);
        if (last == NULL)
        {
            CHECK((got == NULL) || (strcmp(got, "x") == 0));
            continue;
        }
        CHECK(got != NULL && VCD_ValueToU64(got, &v) == VCD_OK && v == last->value);
    }
    VCD_ReaderClose(r);
    memSinkFree(&m);
}

/* ================================================ ENTRY POINT ============================================ */
int main(void)
{
    RUN(test_ids_unique);
    RUN(test_time_helpers);
    RUN(test_golden_output);
    RUN(test_strict_order);
    RUN(test_same_time_collapse);
    RUN(test_redundant_kept_when_disabled);
    RUN(test_sorted_mode);
    RUN(test_values_and_kinds);
    RUN(test_header_options);
    RUN(test_sink_failure);
    RUN(test_file_round_trip);
    RUN(test_reader_foreign_file);
    RUN(test_reader_ambiguous_and_empty);
    RUN(test_reader_errors);
    RUN(test_uart_frame_edges);
    RUN(test_uart_data_signal_and_break);
    RUN(test_uart_round_trip_random);
    RUN(test_sorted_stress);

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
