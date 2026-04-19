#ifndef L2_KWP2000_H
#define L2_KWP2000_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include <stdbool.h>
#include <stdint.h>
#include "datalink.h"
#include <stddef.h>
#include "string.h"

/* ================================================= MACROS ================================================ */
#define SPT_5BAUD_INIT
// #define SPT_FAST_INIT
// #define SPT_CHANGE_TIMING_PARAM
#define FAST_INIT_WAKEUP_START  (0x1)
#define FAST_INIT_WAKEUP_END    (0x2)
#define SLOW_INIT_5BAUD_START   (0x3)
#define SLOW_INIT_5BAUD_END     (0x4)

// Timing
#define KWP_P1_TIME_MIN      (0U)
#define KWP_P1_TIME_MAX      (20U)
#define KWP_P2_TIME_MIN      (25U)
#define KWP_P2_TIME_MAX      (50U)
#define KWP_P2_STAR_TIME_MIN (25U)
#define KWP_P2_STAR_TIME_MAX (5000U)
#define KWP_P3_TIME_MIN      (55U)
#define KWP_P3_TIME_MAX      (5000U)
#define KWP_P4_TIME_MIN      (5U)
#define KWP_P4_TIME_MAX      (20U)

#define PULSE_HIGH (1U)
#define PULSE_LOW (0U)
#define MAX_BLOCKING_RECV_TIME (300U)
/********************************************* SERVICES SECTION **********************************************/
#define START_COMM_REQ \
    ((data_t){ \
        .req = \
        { \
            .sid   = 0x81, \
            .param = {0x0}, \
        },\
        .len   = 0x0, \
    })
/********************************************* SERVICES SECTION **********************************************/

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/********************************************* MESSAGES SECTION **********************************************/
typedef union
{
    uint8_t val;
    struct
    {
        uint8_t len : 6;
        uint8_t a0  : 1;
        uint8_t a1  : 1;
    } bit;
} format_t;

typedef struct
{
    union
    {
        obd_request_t req;
        obd_response_t resp;
    };
    size_t len;
}data_t;

typedef struct
{
    format_t fmt;
    uint8_t trgAddr;
    uint8_t srcAddr;
    uint8_t len;
} header_t;

typedef struct
{
    header_t header;
    data_t data;
    uint8_t cs;
} message_t;

/********************************************* MESSAGES SECTION **********************************************/

typedef union
{
    uint8_t val;
    struct
    {
        uint8_t POR        : 1;
        uint8_t STOP_COMM  : 1;
        uint8_t P3_TIMEOUT : 1;
        uint8_t Reserved   : 4;
        uint8_t CONN_OK    : 1;
    }bits;
}connectionStatus_t;

typedef struct
{
    connectionStatus_t conStatus;
    header_t header;
    uint8_t kb1;
    uint8_t kb2;
}l2_kwp_ctx_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
static inline void SendByteBitBang(dataLink_if_t *self, uint8_t byte, uint8_t baudRate)
{
    const uint16_t delay = (1000 / baudRate);

    // Set line HIGH (idle) and switch to bit-bang mode for 5 baud
    LIBOBD_SwitchMode(self, SLOW_INIT_5BAUD_START);

    // Start bit (LOW)
    LIBOBD_SendPulse(self, PULSE_LOW);
    LIBOBD_Delay(self, delay);

    // Byte
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t bit = (byte >> i) & 0x01;
        LIBOBD_SendPulse(self, bit ? PULSE_HIGH : PULSE_LOW);
        LIBOBD_Delay(self, delay);
    }

    // Stop bit (HIGH)
    LIBOBD_SendPulse(self, PULSE_HIGH);
    LIBOBD_Delay(self, delay);

    // Switch to 10400 baud for response
    LIBOBD_SwitchMode(self, SLOW_INIT_5BAUD_END);
}

static inline obd_status_t ReadByteInTimeframe(dataLink_if_t *self, uint8_t *byte, uint16_t timeMin, uint16_t timeMax)
{
    obd_status_t status = {0};
    uint32_t timeStart = 0;
    uint32_t timeEnd = 0;
    uint32_t timeElapsed = 0;

    timeStart = LIBOBD_GetTimeMs(self);
    LIBOBD_StartTimeout(self, timeMax);

    while (!LIBOBD_ReceiveByte(self, byte))
    {
        status.timeout = OBD_ERR_TIMEOUT_MAX;
        OBD2_ASSERT_EQUAL_OR_EXIT(false, LIBOBD_IsTimeoutExpired(self));
    }

    timeEnd = LIBOBD_GetTimeMs(self);
    LIBOBD_StopTimeout(self);
    timeElapsed = timeEnd - timeStart;

    if (timeElapsed < timeMin)
        status.timeout = OBD_ERR_TIMEOUT_MIN;

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

static inline obd_status_t RecvByteBlocking(dataLink_if_t *self, uint8_t *byte)
{
    obd_status_t status = {0};

    LIBOBD_StartTimeout(self, MAX_BLOCKING_RECV_TIME);

    while (!LIBOBD_ReceiveByte(self, byte))
    {
        status.timeout = OBD_ERR_TIMEOUT_MAX;
        OBD2_ASSERT_EQUAL_OR_EXIT(false, LIBOBD_IsTimeoutExpired(self));
    }

exit:
    return status;
}
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
obd_status_t l2_kwp_connect(dataLink_if_t *pDataLink);
obd_status_t l2_kwp_send_request(dataLink_if_t *self, const obd_request_t *req, size_t len);
obd_status_t l2_kwp_recv_response(dataLink_if_t *self, obd_response_t *resp, size_t* len);

#endif /* L2_KWP2000_H */
