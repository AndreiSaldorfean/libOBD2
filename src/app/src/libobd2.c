/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "libobd2_priv.h"
#include "datalink.h"
#include "iso15031_5.h"
#include "libobd2_services.h"
#include "statusRetCodes.h"
#include "utils.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* ================================================= MACROS ================================================ */
#define LIBOBD2_PIDS_PER_RANGE (32U)   /* PIDs one "PIDs supported" bitmap covers */
#define LIBOBD2_NO_PID_SLOT    (0xFFU) /* sid has no "PIDs supported" */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t sid;          /* OBD service */
    uint8_t reqLen;       /* bytes of the "PIDs supported" request, sid included */
    uint8_t bitmapOffset; /* param[] index of bitmap byte A in the response */
    uint8_t param1;       /* param[1] of the request: O2 sensor for $05, frame for $02, else 0 */
} libobd2_pid_sid_t;

/* ============================================ LOCAL VARIABLES ============================================ */
/* "PIDs supported" request/response layout per sid (ISO 9141 / KWP2000 framing, same as
 * LibOBD2_RequestSupportedTids for $05 $06 $08; $05 asks O2 sensor $01, bank 1 sensor 1).
 * Its PID 00 also probes whether the ECU supports the sid at all
 * ($08 e.g. is mostly only implemented by US vehicles). */
static const libobd2_pid_sid_t pidSids[MAX_NUM_OF_SIDS_WITH_PIDS] = {
    { .sid = SID_SHOW_CURRENT_DATA,       .reqLen = 2, .bitmapOffset = 1, .param1 = 0x00 }, /* 01 pp       -> 41 pp A B C D */
    { .sid = SID_SHOW_FREEZE_FRAME_DATA,  .reqLen = 3, .bitmapOffset = 2, .param1 = 0x00 }, /* 02 pp 00    -> 42 pp fr A B C D */
    { .sid = SID_TEST_RESULTS_O2_SENSORS, .reqLen = 3, .bitmapOffset = 2, .param1 = 0x01 }, /* 05 pp 01    -> 45 pp 01 A B C D */
    { .sid = SID_TEST_RESULTS_OTHER,      .reqLen = 2, .bitmapOffset = 2, .param1 = 0x00 }, /* 06 pp       -> 46 pp xx A B C D */
    { .sid = SID_CONTROL_ONBOARD_SYSTEM,  .reqLen = 7, .bitmapOffset = 2, .param1 = 0x00 }, /* 08 pp 00*5  -> 48 pp xx A B C D */
    { .sid = SID_REQUEST_VEHICLE_INFO,    .reqLen = 2, .bitmapOffset = 2, .param1 = 0x00 }, /* 09 pp       -> 49 pp nr A B C D */
};

/* Row = slot of the sid in pidSids, column = PID range (see LibOBD2_IsPidSupportedByEcu) */
OBD2_STATIC uint32_t supportedPids[MAX_NUM_OF_ECUS][MAX_NUM_OF_SIDS_WITH_PIDS][LIBOBD2_PID_RANGES];

/* Row = slot of the sid in pidSids: true if the ECU answered its "PIDs supported [01-20]" */
OBD2_STATIC bool supportedSids[MAX_NUM_OF_ECUS][MAX_NUM_OF_SIDS_WITH_PIDS];

/* ============================================ GLOBAL VARIABLES =========================================== */
extern const libobd2_service_t libobd2Services[OBD2_MAX_NUM_SERVICES];

/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC uint8_t LibOBD2_GetPidSidSlot(uint8_t sid);
OBD2_STATIC bool LibOBD2_IsPidSupportedByEcu(uint8_t sid, uint8_t pid);
OBD2_STATIC obd_status_t LibOBD2_ReadPidRange(obd_ctx_t *ctx, const libobd2_pid_sid_t *pPidSid, uint8_t range, uint32_t *pBitmap);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: LibOBD2_GetPidSidSlot
* brief: Maps an OBD sid to its row in supportedPids / pidSids.
*        Returns LIBOBD2_NO_PID_SLOT for sids without "supported PIDs".
******************************************/
OBD2_STATIC uint8_t LibOBD2_GetPidSidSlot(uint8_t sid)
{
    uint8_t slot = LIBOBD2_NO_PID_SLOT;
    uint8_t i = 0;

    for (i = 0; i < MAX_NUM_OF_SIDS_WITH_PIDS; i++)
    {
        if (sid == pidSids[i].sid)
        {
            slot = i;
            break;
        }
    }

    return slot;
}

/******************************************
* title: LibOBD2_IsPidSupportedByEcu
* brief: Looks a PID up in the "PIDs supported" bitmaps read at init.
*        supportedPids[..][slot][n] holds the 4 bytes A B C D returned by
*        PID (n * 0x20) packed as (A << 24) | (B << 16) | (C << 8) | D.
*        That word covers PIDs (n * 0x20 + 1) .. (n * 0x20 + 0x20), with
*        the first one in bit 31 (A bit 7) and the last one in bit 0.
*        Nothing is supported for a sid the ECU didn't answer.
******************************************/
OBD2_STATIC bool LibOBD2_IsPidSupportedByEcu(uint8_t sid, uint8_t pid)
{
    bool isSupported = false;
    uint8_t slot = LIBOBD2_NO_PID_SLOT;
    uint8_t pidIdx = 0U;
    uint32_t pidMask = 0U;

    slot = LibOBD2_GetPidSidSlot(sid);

    if ((LIBOBD2_NO_PID_SLOT == slot) || (!supportedSids[0][slot]))
    {
        isSupported = false;
    }
    /* PID 0x00 is the "PIDs supported [01-20]" request itself: available with the sid. */
    else if (0U == pid)
    {
        isSupported = true;
    }
    else
    {
        pidIdx = (uint8_t)((pid - 1U) / LIBOBD2_PIDS_PER_RANGE);
        pidMask = 1UL << (31U - ((pid - 1U) % LIBOBD2_PIDS_PER_RANGE));
        isSupported = (0U != (supportedPids[0][slot][pidIdx] & pidMask));
    }

    return isSupported;
}

/******************************************
* title: LibOBD2_ReadPidRange
* brief: Requests "PIDs supported" PID (range * 0x20) of one sid and packs
*        the 4 bitmap bytes A B C D as (A << 24) | (B << 16) | (C << 8) | D.
******************************************/
OBD2_STATIC obd_status_t LibOBD2_ReadPidRange(obd_ctx_t *ctx, const libobd2_pid_sid_t *pPidSid, uint8_t range, uint32_t *pBitmap)
{
    obd_status_t status = {0};
    obd_data_t req = {0};
    obd_data_t resp = {0};
    const uint8_t *pBytes = NULL;
    size_t len = 0;
    uint8_t pid = 0;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->pDataLink));
    OBD2_DEBUG_ASSERT((NULL != pPidSid) && (NULL != pBitmap) && (LIBOBD2_PID_RANGES > range));

    pid = (uint8_t)(range * LIBOBD2_PIDS_PER_RANGE);

    // Remaining param bytes stay 0 (data bytes of $08)
    req.sid      = pPidSid->sid;
    req.param[0] = pid;
    req.param[1] = pPidSid->param1;

    status = DL_SendRequest(ctx->pDataLink, &req, pPidSid->reqLen);
    OBD2_ASSERT_OK(status);
    status = DL_RecvResponse(ctx->pDataLink, &resp, &len);
    OBD2_ASSERT_OK(status);

    // Must be the positive answer to this exact request, with all 4 bitmap bytes
    status.response = OBD_SRV_NOT_SUPPORTED_BY_ECU;
    OBD2_IF_COND_GOTO_EXIT((pPidSid->sid + LIBOBD2_POSITIVE_RESP_OFFSET) != resp.sid);
    OBD2_IF_COND_GOTO_EXIT(pid != resp.param[0]);
    OBD2_IF_COND_GOTO_EXIT(len < (1U + pPidSid->bitmapOffset + 4U));
    status.response = OBD_STATUS_OK;

    pBytes   = &resp.param[pPidSid->bitmapOffset];
    *pBitmap = ((uint32_t)pBytes[0] << 24) | ((uint32_t)pBytes[1] << 16) |
               ((uint32_t)pBytes[2] << 8)  |  (uint32_t)pBytes[3];

exit:
    return status;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: LibOBD2_IsSidWithAvailablePids
* brief: True for the sids that report their supported PIDs (1 2 5 6 8 9).
******************************************/
bool LibOBD2_IsSidWithAvailablePids(uint8_t sid)
{
    return (LIBOBD2_NO_PID_SLOT != LibOBD2_GetPidSidSlot(sid));
}

/******************************************
* title: LibOBD2_IsSidSupportedByEcu
* brief: False if init found that the ECU doesn't support the sid.
*        Only sids with PIDs can be probed (asking e.g. $04 would clear
*        the DTCs), so sids without PIDs are always reported as supported.
******************************************/
bool LibOBD2_IsSidSupportedByEcu(uint8_t sid)
{
    bool isSupported = true;
    uint8_t slot = LIBOBD2_NO_PID_SLOT;

    slot = LibOBD2_GetPidSidSlot(sid);

    if (LIBOBD2_NO_PID_SLOT != slot)
    {
        isSupported = supportedSids[0][slot];
    }

    return isSupported;
}

/******************************************
* title: LibOBD2_ReadSupportedPids
* brief: Probes every sid with PIDs and reads its "PIDs supported" ranges
*        into supportedSids / supportedPids. A sid is supported if the ECU
*        answers its PID 00 (no answer or a negative response: not
*        supported). A range is only read if the previous one announced it
*        (its last bit). Only a failing $01 PID 00 (mandatory) is an error.
******************************************/
obd_status_t LibOBD2_ReadSupportedPids(obd_ctx_t *ctx)
{
    const obd_status_t okStatus = {0};
    const libobd2_pid_sid_t *pPidSid = NULL;
    obd_status_t status = {0};
    uint32_t bitmap = 0;
    uint8_t slot = 0;
    uint8_t range = 0;
    bool isNextRangeSupported = false;

    OBD2_DEBUG_ASSERT((NULL != ctx) && (NULL != ctx->pDataLink));

    (void)memset(supportedPids, 0, sizeof(supportedPids));
    (void)memset(supportedSids, 0, sizeof(supportedSids));

    for (slot = 0; slot < MAX_NUM_OF_SIDS_WITH_PIDS; slot++)
    {
        pPidSid = &pidSids[slot];
        isNextRangeSupported = true;

        for (range = 0; (range < LIBOBD2_PID_RANGES) && isNextRangeSupported; range++)
        {
            bitmap = 0;
            status = LibOBD2_ReadPidRange(ctx, pPidSid, range, &bitmap);

            if ((OBD_STATUS_OK != status.response) || (OBD_STATUS_OK != status.timeout))
            {
                OBD2_IF_COND_GOTO_EXIT((SID_SHOW_CURRENT_DATA == pPidSid->sid) && (0U == range));
                status = okStatus;
                break;
            }

            supportedSids[0][slot]        = true;
            supportedPids[0][slot][range] = bitmap;
            isNextRangeSupported = (0U != (bitmap & 1U));
        }
    }

exit:
    return status;
}

/******************************************
* title: LibOBD2_IsLibobd2SidSupported
* brief: True if the ECU supports every sid and PID the service requests.
*        Requests to sids without PIDs (e.g. $03) are always allowed.
******************************************/
bool LibOBD2_IsLibobd2SidSupported(libobd2_sid_t libobd2Sid)
{
    const libobd2_data_t *pRequests = NULL;
    bool isSupported = true;
    size_t i = 0;
    uint8_t sid = 0;
    uint8_t pid = 0;

    OBD2_DEBUG_ASSERT(OBD2_MAX_NUM_SERVICES > libobd2Sid);

    pRequests = libobd2Services[libobd2Sid].serviceRequests;

    for (i = 0; (i < LIBOBD2_MAX_SREQ) && (0U != pRequests[i].dataLen) && isSupported; i++)
    {
        sid = pRequests[i].data.sid;
        pid = pRequests[i].data.param[0];

        isSupported = LibOBD2_IsSidSupportedByEcu(sid);

        if (isSupported && LibOBD2_IsSidWithAvailablePids(sid))
        {
            isSupported = LibOBD2_IsPidSupportedByEcu(sid, pid);
        }
    }

    return isSupported;
}
