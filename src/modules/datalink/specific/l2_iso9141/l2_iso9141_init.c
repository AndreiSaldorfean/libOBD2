/* ================================================ INCLUDES =============================================== */
#include "l2_iso9141_priv.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
OBD2_STATIC obd_status_t L2_ISO9141_StoreKwbAndSelectProtocol_Validate(dataLink_if_t *self, uint8_t *protocol);
OBD2_STATIC obd_status_t L2_ISO9141_StoreKwbAndSelectProtocol_Execute(dataLink_if_t *self, uint8_t *protocol, uint8_t kb1, uint8_t kb2);
OBD2_STATIC obd_status_t L2_ISO9141_StoreKwbAndSelectProtocol_Verify(dataLink_if_t *self, uint8_t *protocol);
OBD2_STATIC obd_status_t L2_ISO9141_StoreKwbAndSelectProtocol(dataLink_if_t *self, uint8_t *protocol, uint8_t kb1, uint8_t kb2);

OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit_Validate(dataLink_if_t *self, uint8_t *protocol);
OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit_Execute(dataLink_if_t *self, uint8_t *protocol);
OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit_Verify(dataLink_if_t *self, uint8_t *protocol);
OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit(dataLink_if_t *self, uint8_t* protocol);

OBD2_STATIC obd_status_t L2_ISO9141_Connect_Validate(dataLink_if_t *self, uint8_t *protocol);
OBD2_STATIC obd_status_t L2_ISO9141_Connect_Execute(dataLink_if_t *self, uint8_t *protocol);
OBD2_STATIC obd_status_t L2_ISO9141_Connect_Verify(dataLink_if_t *self, uint8_t *protocol);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/******************************************
* title: L2_ISO9141_StoreKwbAndSelectProtocol_Validate
* brief: Checks the inputs of L2_ISO9141_StoreKwbAndSelectProtocol.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_StoreKwbAndSelectProtocol_Validate(dataLink_if_t *self, uint8_t *protocol)
{
    obd_status_t status = L2_ISO9141_ValidateLinkWithCtx(self);
    OBD2_ASSERT_OK(status);

    if (NULL == protocol)
    {
        status.response = OBD_ERR_NULL_PTR;
    }

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_StoreKwbAndSelectProtocol_Execute
* brief: Stores the keyword bytes in the context and picks the header and
*        protocol that go with them.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_StoreKwbAndSelectProtocol_Execute(dataLink_if_t *self, uint8_t *protocol, uint8_t kb1, uint8_t kb2)
{
    l2_iso9141_ctx_t *ctx = (l2_iso9141_ctx_t *)(self->pProtocolCtx);
    obd_status_t status = {0}; // ok

    // Store keyword bytes for protocol identification
    ctx->kb1 = kb1;
    ctx->kb2 = kb2;

    // ISO9141-2
    // If 0x08 then 0x68 0x6A 0xF1 header is used always with 7 bytes of data max
    if (kb1 == 0x08 && kb2 == 0x08)
    {
        ctx->header = DL_ISO9141_HEADR;
        *protocol   = ISO9141;
    }
    // Can use KWP2000
    else if (kb1 == 0x94 && kb2 == 0x94)
    {
        // Functional addr
        // Can use kwp2000 header and len
        ctx->header = DL_KWP2000_HEADR;
        *protocol   = ISO9141; /* TODO: Change to KWP2000 once support is added for it */
    }
    // Invalid keybytes
    else
    {
        status.response = OBD_ERR_5BAUD_WRONG_KEYBYTES;
        goto exit;
    }

    ctx->protocol = *protocol;
exit:
    return status;
}

/******************************************
* title: L2_ISO9141_StoreKwbAndSelectProtocol_Verify
* brief: Checks that a known protocol was selected and a header was stored.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_StoreKwbAndSelectProtocol_Verify(dataLink_if_t *self, uint8_t *protocol)
{
    l2_iso9141_ctx_t *ctx = (l2_iso9141_ctx_t *)(self->pProtocolCtx);
    obd_status_t status = {0};

    if (((ISO9141 != *protocol) && (KWP2000 != *protocol)) || (0U == ctx->header.fmt))
    {
        status.response = OBD_ERR_PROTOCOL_NOT_FOUND;
    }

    return status;
}

/******************************************
* title: L2_ISO9141_StoreKwbAndSelectProtocol
* brief: Selects the protocol and header from the keyword bytes.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_StoreKwbAndSelectProtocol(dataLink_if_t *self, uint8_t *protocol, uint8_t kb1, uint8_t kb2)
{
    obd_status_t status = {0};

    status = L2_ISO9141_StoreKwbAndSelectProtocol_Validate(self, protocol);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_StoreKwbAndSelectProtocol_Execute(self, protocol, kb1, kb2);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_StoreKwbAndSelectProtocol_Verify(self, protocol);

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_5BaudInit_Validate
* brief: Checks the inputs of L2_ISO9141_5BaudInit.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit_Validate(dataLink_if_t *self, uint8_t *protocol)
{
    obd_status_t status = L2_ISO9141_ValidateLinkWithCtx(self);
    OBD2_ASSERT_OK(status);

    if (NULL == protocol)
    {
        status.response = OBD_ERR_NULL_PTR;
    }

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_5BaudInit_Execute
* brief: Runs the 5-baud wake-up: sends the address, reads sync and keyword
*        bytes, answers with ~KB2 and reads the inverted address back.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit_Execute(dataLink_if_t *self, uint8_t *protocol)
{
    obd_status_t status = {0};
    (void)status;
    uint8_t syncByte = 0;
    uint8_t kb1 = 0;
    uint8_t kb2 = 0;
    uint8_t invAddr = 0;
    const uint8_t targetAddr = 0x33;

    LIBOBD_Delay(self, ISO9141_W5_TIME_MIN);

    // Send address byte at 5 baud rate
    DL_SendByteBitBanged(self, targetAddr, 5);

    // Read Sync byte
    status = DL_ReadByteInTimeframe(self, NULL, &syncByte, ISO9141_W1_TIME_MIN, ISO9141_W1_TIME_MAX);
    status.response = OBD_ERR_5BAUD_SYNC_NOT_RECVD;
    OBD2_ASSERT_NO_TIMEOUT(status);
    status.response = OBD_ERR_5BAUD_WRONG_SYNC_BYTE;
    OBD2_ASSERT_EQUAL_OR_EXIT(0x55, syncByte);

    // Receive KB1 (W2 timing: 5-20ms)
    status = DL_ReadByteInTimeframe(self, NULL, &kb1, ISO9141_W2_TIME_MIN, ISO9141_W2_TIME_MAX);
    status.response = OBD_ERR_5BAUD_KB1_NOT_RECVD;
    OBD2_ASSERT_NO_TIMEOUT(status);

    // Receive KB2 (W3 timing: 0-20ms)
    status = DL_ReadByteInTimeframe(self, NULL, &kb2, ISO9141_W3_TIME_MIN, ISO9141_W3_TIME_MAX);
    status.response = OBD_ERR_5BAUD_KB2_NOT_RECVD;
    OBD2_ASSERT_NO_TIMEOUT(status);

    YIELD;
    // Wait W4 (25-50ms) then send inverted KB2
    LIBOBD_Delay(self, ISO9141_W4_TIME_MIN);
    LIBOBD_SendByte(self, ~kb2);

    // Clear echo
    LIBOBD_FlushRx(self);

    // Receive inverted address from ECU (W4 timing: 25-50ms)
    status = DL_ReadByteInTimeframe(self, NULL, &invAddr, ISO9141_W4_TIME_MIN, ISO9141_W4_TIME_MAX);
    status.response = OBD_ERR_5BAUD_INV_ADDR_NOT_RECVD;
    OBD2_ASSERT_NO_TIMEOUT(status);
    status.response = OBD_ERR_5BAUD_WRONG_INV_ADDR;
    OBD2_ASSERT_EQUAL_OR_EXIT((uint8_t)(~targetAddr), invAddr);

    status = L2_ISO9141_StoreKwbAndSelectProtocol(self, protocol, kb1, kb2);
    OBD2_ASSERT_OK(status);

    memset(&status, 0, sizeof(obd_status_t));
exit:
    return status;
}

/******************************************
* title: L2_ISO9141_5BaudInit_Verify
* brief: No extra check. The last step of Execute,
*        L2_ISO9141_StoreKwbAndSelectProtocol, already verifies the result.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit_Verify(dataLink_if_t *self, uint8_t *protocol)
{
    obd_status_t status = {0};
    (void)self;
    (void)protocol;
    return status;
}

/******************************************
* title: L2_ISO9141_5BaudInit
* brief: Wakes up the ECU with the ISO9141-2 5-baud init and selects the
*        protocol from its keyword bytes.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_5BaudInit(dataLink_if_t *self, uint8_t* protocol)
{
    obd_status_t status = {0};

    status = L2_ISO9141_5BaudInit_Validate(self, protocol);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_5BaudInit_Execute(self, protocol);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_5BaudInit_Verify(self, protocol);

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_Connect_Validate
* brief: Checks the inputs of l2_iso9141_connect.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_Connect_Validate(dataLink_if_t *self, uint8_t *protocol)
{
    obd_status_t status = L2_ISO9141_ValidateLinkWithCtx(self);
    OBD2_ASSERT_OK(status);

    if (NULL == protocol)
    {
        status.response = OBD_ERR_NULL_PTR;
    }

exit:
    return status;
}

/******************************************
* title: L2_ISO9141_Connect_Execute
* brief: Clears the protocol context and runs the 5-baud init.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_Connect_Execute(dataLink_if_t *self, uint8_t *protocol)
{
    l2_iso9141_ctx_t  *ctx = (l2_iso9141_ctx_t *)(self->pProtocolCtx);
    memset(ctx, 0, sizeof(l2_iso9141_ctx_t));
    obd_status_t status = L2_ISO9141_5BaudInit(self, protocol);
    return status;
}

/******************************************
* title: L2_ISO9141_Connect_Verify
* brief: Checks that the connection left a header in the context. Execute
*        clears the context first, so a header means init filled it in.
******************************************/
OBD2_STATIC obd_status_t L2_ISO9141_Connect_Verify(dataLink_if_t *self, uint8_t *protocol)
{
    l2_iso9141_ctx_t *ctx = (l2_iso9141_ctx_t *)(self->pProtocolCtx);
    obd_status_t status = {0};
    (void)protocol;

    if (0U == ctx->header.fmt)
    {
        status.response = OBD_ERR_PROTOCOL_NOT_FOUND;
    }

    return status;
}

/* ================================================ MODULE API ============================================= */
/******************************************
* title: l2_iso9141_connect
* brief: Opens an ISO9141-2 connection to the ECU (5-baud init) and stores
*        the header to use in the protocol context.
******************************************/
obd_status_t l2_iso9141_connect(dataLink_if_t *self, uint8_t* protocol)
{
    obd_status_t status = {0};

    status = L2_ISO9141_Connect_Validate(self, protocol);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_Connect_Execute(self, protocol);
    OBD2_ASSERT_OK(status);
    status = L2_ISO9141_Connect_Verify(self, protocol);

exit:
    return status;
}
