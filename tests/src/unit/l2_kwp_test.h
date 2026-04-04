#ifndef L2_KWP_TEST_H
#define L2_KWP_TEST_H
/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void test_L2_KWP_ComputeChecksum_000(void);
void test_L2_KWP_SendMessage_000(void);
void test_L2_KWP_SendMessage_001(void);
void test_L2_KWP_SendMessage_002(void);
void test_L2_KWP_RecvMessage_000(void);
#if defined(SPT_FAST_INIT)
void test_L2_KWP_SRV_StartCommunication_000(void);
void test_L2_KWP_SRV_SendData_000(void);
void test_L2_KWP_FastInit_000(void);
void test_L2_KWP_IdleBasedOnConnStatus_000(void);
#endif /* SPT_FAST_INIT */
#if defined(SPT_CHANGE_TIMING_PARAM)
void test_L2_KWP_SRV_AccessTimingParameter_000(void);
#endif /* SPT_CHANGE_TIMING_PARAM */
#if defined(SPT_5BAUD_INIT)
void test_L2_KWP_5BaudInit_000(void);
#endif /* SPT_5BAUD_INIT */
void test_L2_KWP_Init_000(void);
void test_L2_KWP_ReadHeader_000(void);
void test_PrepareMessage_000(void);

#endif /* L2_KWP_TEST_H */
