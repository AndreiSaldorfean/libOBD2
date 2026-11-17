/* ================================================ INCLUDES =============================================== */
#include "pbt_support.h"
#include "uart_trace.h"
#include <stdio.h>
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#define MESSAGE_MAX_LEN 256u

/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
static uint64_t envU64(const char *name, uint64_t fallback);
static enum theft_hook_run_pre_res quietRunPre(const struct theft_hook_run_pre_info *info, void *env);
static enum theft_hook_run_post_res quietRunPost(const struct theft_hook_run_post_info *info, void *env);
static enum theft_hook_trial_post_res quietTrialPost(const struct theft_hook_trial_post_info *info, void *env);
static enum theft_hook_counterexample_res silentCounterexample(const struct theft_hook_counterexample_info *info,
                                                               void *env);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
static uint64_t envU64(const char *name, uint64_t fallback)
{
    const char *text = getenv(name);
    char *end = NULL;
    uint64_t value = 0u;

    if ((text == NULL) || (text[0] == '\0'))
    {
        return fallback;
    }
    value = strtoull(text, &end, 0);
    return ((end != NULL) && (*end == '\0')) ? value : fallback;
}

/* theft's defaults print a header, a dot per trial and a summary; the Unity line is enough */
static enum theft_hook_run_pre_res quietRunPre(const struct theft_hook_run_pre_info *info, void *env)
{
    (void)info;
    (void)env;
    return THEFT_HOOK_RUN_PRE_CONTINUE;
}

static enum theft_hook_run_post_res quietRunPost(const struct theft_hook_run_post_info *info, void *env)
{
    (void)info;
    (void)env;
    return THEFT_HOOK_RUN_POST_CONTINUE;
}

static enum theft_hook_trial_post_res quietTrialPost(const struct theft_hook_trial_post_info *info, void *env)
{
    (void)info;
    (void)env;
    return THEFT_HOOK_TRIAL_POST_CONTINUE;
}

static enum theft_hook_counterexample_res silentCounterexample(const struct theft_hook_counterexample_info *info,
                                                               void *env)
{
    (void)info;
    (void)env;
    return THEFT_HOOK_COUNTEREXAMPLE_CONTINUE;
}

/* ================================================ MODULE API ============================================= */
enum theft_run_res PBT_Run(struct theft_run_config *cfg, const char *trace, bool quiet)
{
    enum theft_run_res res = THEFT_RUN_ERROR;

    cfg->seed = envU64("PBT_SEED", (cfg->seed != 0u) ? cfg->seed : PBT_DEFAULT_SEED);
    cfg->trials = (size_t)envU64("PBT_TRIALS", (cfg->trials != 0u) ? cfg->trials : PBT_TRIALS_FAST);
    cfg->hooks.run_pre = (cfg->hooks.run_pre != NULL) ? cfg->hooks.run_pre : quietRunPre;
    cfg->hooks.run_post = (cfg->hooks.run_post != NULL) ? cfg->hooks.run_post : quietRunPost;
    cfg->hooks.trial_post = (cfg->hooks.trial_post != NULL) ? cfg->hooks.trial_post : quietTrialPost;
    if (quiet && (cfg->hooks.counterexample == NULL))
    {
        cfg->hooks.counterexample = silentCounterexample;
    }

    if (trace != NULL)
    {
        (void)UART_TRACE_Begin(trace);
        UART_TRACE_Note(cfg->name);
    }
    res = theft_run(cfg);
    if (!quiet && (res == THEFT_RUN_FAIL))
    {
        printf("[%s] failed with run seed 0x%016llx (rerun: PBT_SEED=0x%016llx)\n",
               (cfg->name != NULL) ? cfg->name : "property", (unsigned long long)cfg->seed,
               (unsigned long long)cfg->seed);
    }
    if (trace != NULL)
    {
        UART_TRACE_End();
    }
    return res;
}

void PBT_ExpectKnownBug(struct theft_run_config *cfg, const char *trace, const char *finding)
{
    char message[MESSAGE_MAX_LEN];
    bool verbose = (envU64("PBT_VERBOSE", 0u) != 0u);
    enum theft_run_res res = PBT_Run(cfg, trace, !verbose);

    switch (res)
    {
        case THEFT_RUN_FAIL:
            (void)snprintf(message, sizeof message, "known bug, still reproduced: %s", finding);
            TEST_IGNORE_MESSAGE(message);
            break;
        case THEFT_RUN_PASS:
            (void)snprintf(message, sizeof message,
                           "no counter-example any more (fixed?), replace PBT_KNOWN_BUG by PBT_ASSERT_HOLDS: %s",
                           finding);
            TEST_FAIL_MESSAGE(message);
            break;
        default:
            TEST_FAIL_MESSAGE("theft run error");
            break;
    }
}
