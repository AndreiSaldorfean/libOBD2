#ifndef FAULT_INJECTION_TEST_H
#define FAULT_INJECTION_TEST_H

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void test_FAULT_Attach_InvalidArgs(void);
void test_FAULT_Uart_WriteWindow(void);
void test_FAULT_Uart_ReadFromByte(void);
void test_FAULT_Uart_XorOnlyAndInactiveRule(void);
void test_FAULT_Uart_Detach(void);
void test_FAULT_Tmr_FailWindow(void);
void test_FAULT_Tmr_FailInit(void);
void test_FAULT_Tmr_ExpireAndShift(void);

#endif /* FAULT_INJECTION_TEST_H */
