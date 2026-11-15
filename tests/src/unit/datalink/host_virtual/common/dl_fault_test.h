#ifndef DL_FAULT_TEST_H
#define DL_FAULT_TEST_H

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void test_DLFAULT_RecvResponse_FramingError(void);
void test_DLFAULT_RecvResponse_OverrunOnFormatByte(void);
void test_DLFAULT_RecvResponse_CorruptedByte(void);
void test_DLFAULT_Connect_ParityOnKeyByte(void);
void test_DLFAULT_Connect_NoiseOnSyncByte(void);
void test_DLFAULT_Connect_TimerInitFails(void);
void test_DLFAULT_SendRequest_TimerStartFails(void);
void test_DLFAULT_RecvResponse_TimerStopFails(void);
void test_DLFAULT_RecvResponse_TimeoutExpiresEarly(void);
void test_DLFAULT_RecvResponse_ClockJumpsBack(void);

#endif /* DL_FAULT_TEST_H */
