#ifndef OBD2_UTILS_H
#define OBD2_UTILS_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"
#include <assert.h>

/* ================================================= MACROS ================================================ */
#if defined(UNIT_TESTS)
#define OBD2_STATIC
#define OBD2_INLINE
#else
#define OBD2_STATIC static
#define OBD2_INLINE inline
#endif /* UNIT_TESTS */

/* Checks a precondition the caller guarantees. Compiled out with NDEBUG (Release builds). */
#define OBD2_DEBUG_ASSERT(cond) assert(cond)

#define LIBOBD_DEC(var) (if ((var - 1) != 0) var--)
#define LIBOBD_INC(var) (var++)

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
#define OBD2_CHECK_NULLPTR(var) \
    do { \
       if (NULL == (var)) { status.response = OBD_ERR_NULL_PTR; goto exit; } \
    }while(0)

#define OBD2_ASSERT_NO_TIMEOUT(status) \
    do { \
       if (OBD_STATUS_OK != status.timeout) goto exit; \
    }while(0)

#define OBD2_ASSERT_OK(status) \
    do { \
       if (OBD_STATUS_OK != (status.response) || (OBD_STATUS_OK != status.timeout)) goto exit; \
    }while(0)

#define OBD2_ASSERT_OK_OR_ERR(status, err) \
    do { \
       if (OBD_STATUS_OK != (status.response)){status.response = err ; goto err; } \
    }while(0)

#define OBD2_ASSERT_EQUAL_GOTO_EXIT(val1, val2) \
    do { \
       if ((val1) == (val2)) goto exit; \
    }while(0)

#define OBD2_ASSERT_EQUAL(val1, val2) \
    do { \
       if ((val1) != (val2)) goto exit; \
    }while(0)

#define OBD2_IF_COND_GOTO_EXIT(cond) \
    do { \
       if (cond) {goto exit;} \
    }while(0)

/* After DL_ReadByteInTimeframe: exits with `err` when the byte did not arrive in
 * time, or with the UART error the byte was received with. */
#define OBD2_ASSERT_BYTE_RECVD(status, err) \
    do { \
       if (OBD_STATUS_OK != (status).timeout) { (status).response = (err); goto exit; } \
       if (OBD_STATUS_OK != (status).response) { goto exit; } \
    }while(0)

#define OBD2_ASSERT_EQUAL_OR_EXIT(val1, val2) \
    do { \
       if ((val1) != (val2)) {goto exit;} \
    }while(0)

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* OBD2_UTILS_H */

