#ifndef L2_ISO9141_TEST_H
#define L2_ISO9141_TEST_H

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void test_L2_ISO9141_ComputeChecksum_000(void);
void test_L2_ISO9141_SendMessage_000(void);
void test_L2_ISO9141_RecvMessage_000(void);
void test_L2_ISO9141_ReadHeader_000(void);
void test_L2_ISO9141_PrepareMessage_000(void);
void test_L2_ISO9141_5BaudInit_000(void);
void test_l2_ISO9141_connect_000(void);
void test_l2_ISO9141_send_request_000(void);
void test_l2_ISO9141_recv_response_000(void);

#endif /* L2_ISO9141_TEST_H */
