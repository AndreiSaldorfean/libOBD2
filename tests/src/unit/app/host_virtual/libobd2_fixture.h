#ifndef LIBOBD2_FIXTURE_H
#define LIBOBD2_FIXTURE_H

/*************************************************************************************************************
 * Shared virtual K-line for the app (LibOBD2_*) tests (positive/ and negative/).
 *
 * The tester is already connected over ISO9141 (header 68 6A F1); the ECU answers are queued on the line
 * ahead of time with the virtual time they arrive at, then the clock is rewound and the request runs.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "iso9141.h"
#include "libobd2.h"
#include "libobd2_services.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define APPFX_TIME_STEP        10u /* virtual ms consumed by every UART poll of the tester */
#define APPFX_ECU_BYTE_GAP     5u  /* ECU inter-byte time (P1), <= the poll step so every byte is ready when polled */
#define APPFX_HDR_LEN          3u
#define APPFX_RESPONSE_BUF_LEN 64u
#define APPFX_VIN_LEN          17u
#define APPFX_VIN_MSG_CNT      5u  /* 17 VIN chars + 3 pad bytes, 4 data bytes per message */

/* Time the tester needs to send a request of `dataLen` bytes (sid + params): P3 idle, then P4 after each byte */
#define APPFX_REQ_DURATION(dataLen) (P3_TIME_MIN + (APPFX_HDR_LEN + (dataLen) + 1u) * P4_TIME_MIN)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    virtTmrCtx_t      timer;
    virtUartLinkCtx_t uart;
    iso9141_ctx_t     proto;
    dataLink_if_t     link;
    obd_ctx_t         ctx;
} appFixture_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
extern appFixture_t appFx;

/* The 20 data bytes of the APPFX_VIN_MSG_CNT VIN messages: 3 pad bytes + "1HGCM82633A004352" */
extern const uint8_t appFxVin[APPFX_VIN_MSG_CNT * 4u];

/* =============================================== MODULE API ============================================== */

/* Clock at 0, empty line, tester connected over ISO9141 with timer/transport inits that keep both
 * (DL_Connect would otherwise zero timeStep and drop the queued ECU bytes). */
void APPFX_SetUpLine(void);

/* True when one of the 'dataLen' data bytes of an ECU frame 48 6B 12 <data> <cs> equals the sum of the
 * bytes before it: a receiver that frames by checksum ends the frame there (PROPERTY_TESTS.md, F1) */
bool APPFX_HasEarlyChecksum(const uint8_t *data, size_t dataLen);

/* A data link whose send/recv are test doubles: 'ctx' becomes pProtocolCtx, and the driver pointers are
 * set to placeholders the doubles never use (DL_ValidateLink rejects a link without them). */
dataLink_if_t APPFX_MockLink(void *ctx, dl_send_request_t send, dl_recv_response_t recv);

/* Makes the tester's transceiver suppress its own echo until the next APPFX_SetUpLine(): sent bytes are
 * still drawn on the trace's wire, but never land in the receive queue. The virtual line is a loopback
 * and its flush_rx is a no-op, so without this a silent ECU looks like an ECU answering too early (the
 * tester reads its own request back). */
void APPFX_SuppressEcho(void);

/* Queues one ECU frame 48 6B 12 <data> <cs>: the format byte P2 min after 'reqEnd' (the virtual time at
 * which the tester stopped talking), the rest APPFX_ECU_BYTE_GAP apart. Returns the virtual time at
 * which the tester has read the whole frame. */
uint32_t APPFX_QueueEcuFrame(uint32_t reqEnd, const uint8_t *data, size_t dataLen);

/* Queues the ECU side of a SID $09 exchange: the count answer 49 <cntPid> <msgCnt>, then the first
 * 'sentCnt' of the data messages 49 <pid> <msgNr> D1..D4 (payload: 4 bytes per message). */
void APPFX_QueueVehicleInfo(uint8_t cntPid, uint8_t pid, const uint8_t *payload, uint8_t msgCnt, uint8_t sentCnt);

/* Queues what the simulated ECU (the answer table in ecu/app/ecu_libobd2_services.h) sends back for every
 * request of 'service', each answer after the tester's request, the way the ECU of the stm32 examples
 * answers. Requests the ECU does not answer get nothing. Returns the virtual time at which the tester has
 * read the last answer. Services that read until P2 max expires also need APPFX_SuppressEcho(): with the
 * echo the tester would read its own request back as one more answer. */
uint32_t APPFX_QueueEcuExchange(const libobd2_service_t *service);

/* True when one of the simulated ECU's answers to 'service' has a data byte equal to the running sum
 * (APPFX_HasEarlyChecksum): the tester cuts that frame short (PROPERTY_TESTS.md, F1) */
bool APPFX_EcuExchangeHitsF1(const libobd2_service_t *service);

#endif /* LIBOBD2_FIXTURE_H */
