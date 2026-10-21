#ifndef DL_TEST_H
#define DL_TEST_H

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void test_DL_ReadByteInTimeframe_000(void);
void test_DL_ReadByteInTimeframe_TooEarly(void);
void test_DL_ReadByteInTimeframe_NoByte(void);
void test_DL_RecvByteBlocking_000(void);
void test_DL_RecvByteBlocking_NoByte(void);
void test_DL_SendByteBitBanged_000(void);
void test_DL_Connect_000(void);
void test_DL_SendRecv_000(void);

#endif /* DL_TEST_H */
