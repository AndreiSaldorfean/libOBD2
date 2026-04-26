#ifndef OBD2_UTILS_H
#define OBD2_UTILS_H

/* ================================================ INCLUDES =============================================== */
#include "statusRetCodes.h"

/* ================================================= MACROS ================================================ */
#if defined(UNIT_TESTS)
#define OBD2_STATIC
#define OBD2_INLINE
#else
#define OBD2_STATIC static
#define OBD2_INLINE inline
#endif /* DEBUG */

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

#define OBD2_ASSERT_EQUAL(val1, val2) \
    do { \
       if ((val1) != (val2)) goto exit \
    }while(0)

#define OBD2_IF_COND_GOTO_EXIT(cond) \
    do { \
       if (cond) {goto exit;} \
    }while(0)

#define OBD2_ASSERT_EQUAL_OR_EXIT(val1, val2) \
    do { \
       if ((val1) != (val2)) {goto exit;} \
    }while(0)

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* OBD2_UTILS_H */
