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
/* Returns the number of failed tests (0 = all passed). */
int targetRunTests(void);

#endif /* TARGET_TEST_FIXTURES_H */

