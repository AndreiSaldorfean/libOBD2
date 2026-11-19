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
#define APPFX_ECU_MAX_ANSWERS  16u /* answers one scripted ECU knows */
#define APPFX_ECU_MAX_REQS     32u /* requests one scripted ECU records */

/* Virtual time at which the tester is connected after APPFX_QueueIso9141Init's 5-baud init: the ECU
 * sends ~address (its last byte) here and the tester reads it one poll later */
#define APPFX_INIT_CONNECT_END (ISO9141_W5_TIME_MIN + 2000u + ISO9141_W1_TIME_MIN + ISO9141_W2_TIME_MIN + \
                                ISO9141_W3_TIME_MIN + 3u * ISO9141_W4_TIME_MIN + APPFX_TIME_STEP)

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

/* An ECU behind APPFX_MockLink that answers requests by (sid, first param) from a script and records
 * every request. A request without a scripted answer gets silentStatus if customSilence is set, else
 * OBD_ERR_COMM_FMT_BYTE_NOT_RECVD with the P2 max timeout, like a silent ECU. Every send returns
 * sendStatus (OK after APPFX_EcuReset). With useSimulator, requests the script does not know get the
 * first message the ECU simulator answers with (ECU_LibOBD2_GetAnswer). */
typedef struct
{
    struct
    {
        uint8_t    sid;
        uint8_t    pid;
        obd_data_t resp;
        size_t     respLen;
    } answers[APPFX_ECU_MAX_ANSWERS];
    size_t answerCnt;

    obd_status_t sendStatus;
    obd_status_t silentStatus;
    bool         customSilence;
    bool         useSimulator;

    obd_data_t reqs[APPFX_ECU_MAX_REQS];
    size_t     reqLens[APPFX_ECU_MAX_REQS];
    size_t     reqCnt;
} appFxScriptedEcu_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
extern appFixture_t appFx;

/* The 20 data bytes of the APPFX_VIN_MSG_CNT VIN messages: 3 pad bytes + "1HGCM82633A004352" */
extern const uint8_t appFxVin[APPFX_VIN_MSG_CNT * 4u];

/* =============================================== MODULE API ============================================== */

/* Clock at 0, empty line, tester connected over ISO9141 with timer/transport inits that keep both
 * (DL_Connect would otherwise zero timeStep and drop the queued ECU bytes). The context counts as
 * initialised and the ECU as supporting every sid and PID, as if LibOBD2_Init had run. */
void APPFX_SetUpLine(void);

/* Sets what LibOBD2_Init would have learnt: every sid with PIDs and every PID supported, or none */
void APPFX_SetEcuSupportsAll(bool isSupported);

/* For mock-link tests: a context on 'link' that counts as initialised, with an ECU supporting every
 * sid and PID (APPFX_SetEcuSupportsAll) */
obd_ctx_t APPFX_ConnectedCtx(dataLink_if_t *link);

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

/* Queues the ECU side of the ISO9141 5-baud init (sync 0x55, KB1 = KB2 = 0x08, ~address 0xCC) and
 * clears what the link knows about the protocol, so LibOBD2_Init runs the whole connect. The tester is
 * connected at APPFX_INIT_CONNECT_END. */
void APPFX_QueueIso9141Init(void);

/* Empties the script and the recorded requests of 'ecu' */
void APPFX_EcuReset(appFxScriptedEcu_t *ecu);

/* Scripts the answer 'resp' (sid + params, 'respLen' bytes) to request <reqSid> <reqPid> ... */
void APPFX_EcuAnswer(appFxScriptedEcu_t *ecu, uint8_t reqSid, uint8_t reqPid, const uint8_t *resp, size_t respLen);

/* A mock data link driven by 'ecu' (see appFxScriptedEcu_t) */
dataLink_if_t APPFX_EcuLink(appFxScriptedEcu_t *ecu);

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
