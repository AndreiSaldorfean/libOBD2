#ifndef TARGET_TEST_FIXTURES_H
#define TARGET_TEST_FIXTURES_H

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void targetSetUp();
void targetTearDown();
void targetPreTestsInit();
/* Starts the test task and the scheduler; does not return on the target. */
int targetRunTests(void);

#endif /* TARGET_TEST_FIXTURES_H */

