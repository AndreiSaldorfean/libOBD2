#ifndef OBD2_UTILS_H
#define OBD2_UTILS_H

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"

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
       if (NULL == (var)) return OBD_ERR_NULL_PTR; \
    }while(0)

#define OBD2_ASSERT_OK(val) \
    do { \
       if (OBD_STATUS_OK != (val)) return (val); \
    }while(0)

#define OBD2_ASSERT_EQUAL(val1, val2) \
    do { \
       if ((val1) != (val2)) return (val1); \
    }while(0)

#define OBD2_ASSERT_EQUAL_OR_ERR(val1, val2, err) \
    do { \
       if ((val1) != (val2)) return (err); \
    }while(0)

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* OBD2_UTILS_H */
