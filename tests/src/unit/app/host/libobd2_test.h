#ifndef LIBOBD2_TEST_H
#define LIBOBD2_TEST_H

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void test_LIBOBD2_Init_NullCtx(void);
void test_LIBOBD2_Init_NullDataLink(void);
void test_LIBOBD2_Init_000(void);
void test_LIBOBD2_RequestService_NullResponse(void);
void test_LIBOBD2_RequestService_NullResponseLen(void);
void test_LIBOBD2_RequestService_InvalidSid(void);
void test_LIBOBD2_RequestService_VIN_000(void);
void test_LIBOBD2_RequestService_CALID_000(void);
void test_LIBOBD2_RequestService_CoolantTemp_000(void);

#endif /* LIBOBD2_TEST_H */

