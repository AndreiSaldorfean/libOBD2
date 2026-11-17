#ifndef PBT_SUPPORT_H
#define PBT_SUPPORT_H

/*************************************************************************************************************
 * Shared helpers for the property-based tests (theft). See tests/src/unit/PROPERTY_TESTS.md.
 *
 *   - PBT_Run():           runs a property with the project defaults (trials, seed, quiet output) and
 *                          records the virtual K-line into test_results/<trace>.
 *   - PBT_ASSERT_HOLDS():  the property must hold; a counter-example fails the Unity test.
 *   - PBT_KNOWN_BUG():     the property states correct behaviour the library does not have yet. While
 *                          theft finds a counter-example the test is reported IGNORED with the finding;
 *                          once no counter-example is found the test FAILS, so the marker is removed
 *                          together with the fix.
 *   - PBT_Range()/...:     generator helpers that shrink towards the low end of their range.
 *
 * Environment (all optional):
 *   PBT_SEED=<n>     run with another seed (default: fixed, so CI runs are reproducible)
 *   PBT_TRIALS=<n>   trials per property (default: each property's own count)
 *   PBT_VERBOSE=1    also print the counter-examples of known bugs
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "theft.h"
#include "unity.h"
#include <stdbool.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define PBT_DEFAULT_SEED   0x6C69624F42443221ull /* "libOBD2!" */
#define PBT_TRIALS_FAST    500u                  /* pure functions, a few virtual ms per trial */
#define PBT_TRIALS_LINE    200u                  /* whole exchanges on the virtual K-line */

#define PBT_ASSERT_HOLDS(cfg, trace) \
    TEST_ASSERT_EQUAL_MESSAGE(THEFT_RUN_PASS, PBT_Run((cfg), (trace), false), \
                              "property violated, shrunk counter-example printed above")

#define PBT_KNOWN_BUG(cfg, trace, finding) PBT_ExpectKnownBug((cfg), (trace), (finding))

/* Every generated case struct is plain data: theft's generic free works for all of them */
#define PBT_TYPE_INFO(allocFn, printFn)                                                                     \
    {                                                                                                       \
        .alloc = (allocFn), .free = theft_generic_free_cb, .print = (printFn),                             \
        .autoshrink_config = { .enable = true },                                                            \
    }

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* Uniform in [lo, hi]; shrinks towards lo */
static inline uint32_t PBT_Range(struct theft *t, uint32_t lo, uint32_t hi)
{
    return lo + (uint32_t)theft_random_choice(t, (uint64_t)hi - lo + 1u);
}

static inline uint8_t PBT_Byte(struct theft *t)
{
    return (uint8_t)theft_random_bits(t, 8);
}

/* True with probability 1/n; shrinks towards false */
static inline bool PBT_OneIn(struct theft *t, uint32_t n)
{
    return (n - 1u) == theft_random_choice(t, n);
}

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

/* Runs 'cfg' with the defaults above filled in (fields the caller set are kept) and the K-line traced
 * into 'trace' (NULL: no trace). 'quiet' also suppresses the counter-example print. */
enum theft_run_res PBT_Run(struct theft_run_config *cfg, const char *trace, bool quiet);

/* Body of PBT_KNOWN_BUG(), see above */
void PBT_ExpectKnownBug(struct theft_run_config *cfg, const char *trace, const char *finding);

#endif /* PBT_SUPPORT_H */
