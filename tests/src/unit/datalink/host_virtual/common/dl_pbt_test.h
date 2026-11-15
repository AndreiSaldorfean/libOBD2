#ifndef DL_PBT_TEST_H
#define DL_PBT_TEST_H

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void test_DL_PBT_ReadByteInTimeframe_ReturnsByteWithinWindow(void);
void test_DL_PBT_ReadByteInTimeframe_TimesOutWhenByteNeverArrives(void);
void test_DL_PBT_RecvByteBlocking_ReturnsPushedByte(void);

#endif /* DL_PBT_TEST_H */
