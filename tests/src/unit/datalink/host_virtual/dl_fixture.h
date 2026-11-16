#ifndef DL_FIXTURE_H
#define DL_FIXTURE_H

/*************************************************************************************************************
 * Shared virtual K-line for the datalink tests (positive/ and negative/).
 *
 * One tester link (ISO9141, header 68 6A F1) on the host virtual UART/timer ports, plus helpers that put
 * ECU bytes on the line ahead of time. The ECU side is not simulated: its bytes are queued with the
 * virtual time at which they arrive, then the clock is rewound and the code under test runs.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "datalink.h"
#include "iso9141.h"
#include "libobd2_timer_port.h"
#include "libobd2_uart_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ================================================= MACROS ================================================ */
#define DLFX_TIME_STEP        10u          /* virtual ms consumed by every UART poll */
#define DLFX_ECU_BYTE_GAP     5u           /* ECU inter-byte time (P1), below the poll step */
#define DLFX_TARGET_ADDR      0x33u        /* 5-baud wake-up address */
#define DLFX_BITBANG_BITS     10u          /* start + 8 data + stop */
#define DLFX_BITBANG_BIT_TIME (1000u / 5u) /* one bit at 5 baud */
#define DLFX_MODE_SEQ_LEN     4u
#define DLFX_NO_LATE_BYTE     0xFFu

/* ECU side of a correct 5-baud init: sync 55, keyword bytes 08 08, inverted address CC */
#define DLFX_ECU_INIT_OK                                                                                    \
    ((dlFxEcuInit_t){ .sync = 0x55u, .kb1 = 0x08u, .kb2 = 0x08u, .invAddr = (uint8_t)(~DLFX_TARGET_ADDR),  \
                      .lateIdx = DLFX_NO_LATE_BYTE })

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    virtTmrCtx_t      timer;
    virtUartLinkCtx_t uart;
    iso9141_ctx_t     proto;
    dataLink_if_t     link;

    /* What went through dlFxUartOps.send_pulse / switch_mode (DLFX_SetUpConnectedLine only) */
    uint8_t  pulseLevel[DLFX_BITBANG_BITS + 1u];
    uint32_t pulseTime[DLFX_BITBANG_BITS + 1u];
    size_t   pulseCnt;
    uint8_t  modeSeq[DLFX_MODE_SEQ_LEN];
    size_t   modeCnt;
} dlFixture_t;

/* What the ECU answers to the 5-baud address, and which byte (0 sync .. 3 inverted address) comes late */
typedef struct
{
    uint8_t  sync;
    uint8_t  kb1;
    uint8_t  kb2;
    uint8_t  invAddr;
    uint8_t  lateIdx; /* DLFX_NO_LATE_BYTE: all on time */
    uint32_t lateMs;  /* the late byte and every one after it come this much later */
} dlFxEcuInit_t;

/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
extern dlFixture_t dlFx;

/* DL_Connect() re-initialises the timer and the transport. On the virtual line that would zero timeStep
 * and drop the ECU bytes the test queued, so these ops keep both; everything else is the host port.
 * Tests may swap single ops, but must put them back before they end. */
extern obd_timing_ops_t dlFxTmrOps;
extern obd_uart_ops_t   dlFxUartOps;

/* =============================================== MODULE API ============================================== */

/* Clock at 0, empty line, ISO9141 already connected (send/recv bound, kb 08 08), dlFxTmrOps/dlFxUartOps,
 * bit-bang recording cleared. */
void DLFX_SetUpConnectedLine(void);

/* Clock at 0, empty line, real host ports (tmr_ops/uart_ops), no protocol function bound. */
void DLFX_SetUpLine(void);

/* Puts one byte on the line at virtual time 'time' without moving the clock. */
void DLFX_LineWriteAt(uint32_t time, uint8_t byte);

/* Puts an ECU frame on the line from just after P2 min, DLFX_ECU_BYTE_GAP apart, then rewinds the clock
 * and starts P2 as a request that just ended would. */
void DLFX_QueueEcuFrame(const uint8_t *bytes, size_t len);

/* Puts the ECU side of a 5-baud init on the line, timed for a tester that starts at t=0 (W5, 10 bits at
 * 5 baud, then W1..W4), and rewinds the clock. */
void DLFX_QueueEcu5BaudInit(const dlFxEcuInit_t *ecu);

/* Op/hook helpers */
obd_tmr_status_t DLFX_KeepTimer(void *pHandle);                       /* timer_init that keeps the clock */
bool DLFX_KeepLine(void *handle);                                     /* uart init that keeps the line */
void DLFX_FailStartTimeout(void *pUserData, virtTmrEvent_t *event);   /* onOp: start_timeout fails */
void DLFX_InjectFramingError(void *pUserData, uartEvent_t *event);    /* onWrite: every byte framing error */

#endif /* DL_FIXTURE_H */
