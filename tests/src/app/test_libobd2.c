/* ================================================ INCLUDES =============================================== */
#include "test_libobd2.h"
#include "data_link_if.h"
#include "libobd2.h"
#include "srv_status.h"
#include "transport_if.h"
#include "iso15031_5.h"
#include "tusb.h"
#include "unity.h"
#include <stdio.h>

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_LIBOBD2_0(void)
{
    obd_status_t status;

    obd_transport_ops_t transportOps =
    {
        .init      = NULL,
        .send_byte = NULL,
        .recv_byte = NULL
    };

    dataLink_if_t dataLink =
    {
        .pTransportHandle = NULL,
        .pTransportOps = &transportOps,
        .connect = NULL,
        .send_request = NULL,
        .recv_response = NULL,
    };

    obd_ctx_t ctx =
    {
            .pDataLink = &dataLink,
            .pDataLinkHandle = NULL,
            .connectionStatus = 0
    };

    status = LibOBD2_Init(&ctx);
    TEST_ASSERT_EQUAL_UINT32(OBD_ERR_NULL_PTR, status);
}

void test_LIBOBD2_1(void)
{
    uint32_t respBuffer[256];

    obd_transport_ops_t transportOps =
    {
        .init      = NULL,
        .send_byte = NULL,
        .recv_byte = NULL
    };
    dataLink_if_t dataLink =
    {
        .pTransportHandle = NULL,
        .pTransportOps = &transportOps,
        .connect = NULL,
        .send_request = NULL,
        .recv_response = NULL,
    };
    obd_ctx_t ctx =
    {
        .pDataLink = &dataLink,
        .pDataLinkHandle = NULL,
        .connectionStatus = 0
    };
    size_t len;
    obd_status_t status;

    obd_request_t request = {
        .sid = SID_SHOW_CURRENT_DATA,
        .param = {PID_01_COOLANT_TEMP}
    };

    obd_response_t response = {0};

    status = LibOBD2_RequestService(&ctx, &request, 2, &response, &len);
    TEST_ASSERT_EQUAL_UINT32(OBD_ERR_NULL_PTR, status);
}
