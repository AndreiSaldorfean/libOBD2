#ifndef SRV_STATUS_H
#define SRV_STATUS_H

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define SRV_STATUS_GENERIC            (0x01)
#define OBD_MAKE_ERROR(module, code) ((uint16_t)(((module) << 8) | ((code) & 0xFF)))
#define OBD_STATUS_OK                ((uint16_t)0x0U)
#define OBD_GENERIC_ERROR            (OBD_MAKE_ERROR(SRV_STATUS_GENERIC, (uint16_t)0x1U))
#define OBD_ERR_NULL_PTR             (OBD_MAKE_ERROR(SRV_STATUS_GENERIC, (uint16_t)0x2U))
#define OBD_NOT_SUPPORTED            (OBD_MAKE_ERROR(SRV_STATUS_GENERIC, (uint16_t)0x3U))
#define OBD_ERR_TIMEOUT_MAX          (OBD_MAKE_ERROR(SRV_STATUS_GENERIC, (uint16_t)0x4U))
#define OBD_ERR_TIMEOUT_MIN          (OBD_MAKE_ERROR(SRV_STATUS_GENERIC, (uint16_t)0x5U))
#define OBD_ERR_INVALID_PARAM        (OBD_MAKE_ERROR(SRV_STATUS_GENERIC, (uint16_t)0x6U))

// Init status codes
#define SRV_STATUS_INIT                   (0x02)
#define OBD_ERR_5BAUD_WRONG_SYNC_BYTE     (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x1U))
#define OBD_ERR_5BAUD_WRONG_INV_ADDR      (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x2U))
#define OBD_ERR_5BAUD_WRONG_KEYBYTES      (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x3U))
#define OBD_ERR_5BAUD_KB1_NOT_RECVD       (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x4U))
#define OBD_ERR_5BAUD_KB2_NOT_RECVD       (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x5U))
#define OBD_ERR_5BAUD_SYNC_NOT_RECVD      (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x6U))
#define OBD_ERR_5BAUD_INV_ADDR_NOT_RECVD  (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x7U))
#define OBD_ERR_CONN_TIMER_INIT_FAILED    (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x8U))
#define OBD_ERR_CONN_TRANS_INIT_FAILED    (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0x9U))
#define OBD_ERR_PROTOCOL_NOT_FOUND        (OBD_MAKE_ERROR(SRV_STATUS_INIT, (uint16_t)0xAU))

// Comm status codes
#define SRV_STATUS_COMM                        (0x03)
#define OBD_ERR_COMM_P2_TIMEOUT_MAX_TESTER_ECU (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x1U))
#define OBD_ERR_COMM_P2_TIMEOUT_MIN_TESTER_ECU (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x2U))
#define OBD_ERR_COMM_P3_TIMEOUT_MAX_ECU_TESTER (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x3U))
#define OBD_ERR_COMM_P3_TIMEOUT_MIN_ECU_TESTER (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x4U))
#define OBD_ERR_COMM_FMT_BYTE_NOT_RECVD        (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x5U))
#define OBD_ERR_COMM_TRGT_BYTE_NOT_RECVD       (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x6U))
#define OBD_ERR_COMM_SRC_BYTE_NOT_RECVD        (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x7U))
#define OBD_ERR_COMM_LEN_BYTE_NOT_RECVD        (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x8U))
#define OBD_ERR_COMM_DATA_BYTE_NOT_RECVD       (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0x9U))
#define OBD_ERR_COMM_CS_BYTE_NOT_RECVD         (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0xAU))
#define OBD_ERR_COMM_SEND_MSG_FAILED           (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0xBU))
#define OBD_ERR_COMM_RECV_MSG_FAILED           (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0xCU))
#define OBD_ERR_COMM_ECU_RESPONSE_7F           (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0xCU))
#define OBD_ERR_COMM_ECU_CONNECTION_CLSD       (OBD_MAKE_ERROR(SRV_STATUS_COMM, (uint16_t)0xDU))

// Transport status codes
#define SRV_STATUS_TRANSPORT               (0x04)
#define OBD_RECV_NOT_READY                 (OBD_MAKE_ERROR(SRV_STATUS_TRANSPORT, (uint16_t)0x1U))
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint16_t timeout;
    uint16_t response;
}obd_status_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* SRV_STATUS_H */
