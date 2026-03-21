/* ================================================ INCLUDES =============================================== */
#include "libobd2.h"
#include "srv_status.h"
#include "transport_if.h"
#include "iso15031_5.h"
#include "tusb.h"
#include "unity.h"
#include <stdio.h>
#include <time.h>
#include "kwp_timer.h"
#include "utils.h"
#include "l2_kwp.h"

/* ================================================= MACROS ================================================ */
/* ============================================ LOCAL VARIABLES ============================================ */
/* ============================================ GLOBAL VARIABLES =========================================== */
/* ======================================= LOCAL FUNCTION DECLARATIONS ===================================== */
extern uint8_t L2_KWP_ComputeChecksum(header_t header, data_t data);
extern obd_status_t L2_KWP_SendMessage(dataLink_if_t *self, uint8_t *msg, size_t len);
extern obd_status_t L2_KWP_RecvMessage(dataLink_if_t *self, message_t *recvdMsg);
#if defined(SPT_FAST_INIT)
extern obd_status_t L2_KWP_SRV_StartCommunication(dataLink_if_t *self);
extern obd_status_t L2_KWP_SRV_SendData( dataLink_if_t *self, message_t *sentMsg, message_t *recvdMsg);
extern obd_status_t L2_KWP_FastInit(dataLink_if_t *self);
extern void L2_KWP_IdleBasedOnConnStatus(dataLink_if_t *self);
#endif /* SPT_FAST_INIT */
#if defined(SPT_CHANGE_TIMING_PARAM)
extern obd_status_t L2_KWP_SRV_AccessTimingParameter(dataLink_if_t *self);
#endif /* SPT_CHANGE_TIMING_PARAM */
#if defined(SPT_5BAUD_INIT)
extern obd_status_t L2_KWP_5BaudInit(dataLink_if_t *self);
#endif /* SPT_5BAUD_INIT */
extern obd_status_t L2_KWP_Init(dataLink_if_t *self);
extern obd_status_t L2_KWP_ReadHeader(dataLink_if_t *self, header_t *header, size_t *headerLen);
extern void L2_KWP_PrepareMessage(message_t *sentMsg, uint8_t *aSentMsg, size_t *len);

/* ======================================== LOCAL FUNCTION DEFINITIONS ===================================== */
/* ================================================ MODULE API ============================================= */

// ==========================================================================================================
// Description: This test shall validate the checksum of an input message
// Type: Positive
// Steps:
//  - Input dummy message
//  - Call L2_KWP_ComputeChecksum with dummy data
//  - Assert checksum
// ==========================================================================================================
void test_L2_KWP_ComputeChecksum_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_SendMessage_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_RecvMessage_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

#if defined(SPT_FAST_INIT)
// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_SRV_StartCommunication_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_SRV_SendData_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_FastInit_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_IdleBasedOnConnStatus_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

#endif /* SPT_FAST_INIT */
#if defined(SPT_CHANGE_TIMING_PARAM)
// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_SRV_AccessTimingParameter_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

#endif /* SPT_CHANGE_TIMING_PARAM */
#if defined(SPT_5BAUD_INIT)
// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_5BaudInit_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

#endif /* SPT_5BAUD_INIT */
// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_Init_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_L2_KWP_ReadHeader_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_PrepareMessage_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_l2_kwp_connect_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_l2_kwp_send_request_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

// ==========================================================================================================
// Description:
// Type:
// Steps:
// ==========================================================================================================
void test_l2_kwp_recv_response_000(void)
{
    uint32_t expected = 0;
    uint32_t actual = 0;

    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

