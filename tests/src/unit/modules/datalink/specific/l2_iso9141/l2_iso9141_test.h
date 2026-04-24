#ifndef L2_ISO9141_TEST_H
#define L2_ISO9141_TEST_H

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "kwp_timer.h"
#include "l2_kwp2000.h"
#include "libobd2.h"
#include "uart_kwp_transport_port.h"

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
extern const header_t iso9141_header_00_ecu;
extern const header_t iso9141_header_00;
extern const obd_request_t iso9141_request_01;
extern const obd_request_t iso9141_request_02;
extern const obd_request_t iso9141_request_00;
extern const data_t iso9141_data_00;
extern const message_t iso9141_msg_00;
extern const message_t iso9141_msg_00_ecu;

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
