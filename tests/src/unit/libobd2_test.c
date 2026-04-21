/* ================================================ INCLUDES =============================================== */
#include "libobd2_test.h"
#include "libobd2.h"
#include "libobd2_test_utils.h"
#include "srv_status.h"
#include "unity.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern obd_status_t LibOBD2_Init(obd_ctx_t *ctx);
extern obd_status_t LibOBD2_RequestService(
    obd_ctx_t *ctx,
    const obd_request_t* request,
    size_t requestLen,
    obd_response_t* response,
    size_t* responseLen);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */
void test_LibOBD2_Init_000(void)
{
    obd_status_t status;
    status = LibOBD2_Init(&ctx);
    TEST_ASSERT_EQUAL(OBD_STATUS_OK, status);

}

void test_LibOBD2_RequestService_000(void)
{
    TEST_ASSERT(1);
}
