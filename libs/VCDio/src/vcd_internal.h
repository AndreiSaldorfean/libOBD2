#ifndef VCD_INTERNAL_H
#define VCD_INTERNAL_H

/*************************************************************************************************************
 * VCDio private helpers. Not part of the public API.
 *
 * The allocator can be replaced by defining VCD_MALLOC, VCD_REALLOC and VCD_FREE (all three) when
 * compiling the library, e.g. -DVCD_MALLOC=myMalloc -DVCD_REALLOC=myRealloc -DVCD_FREE=myFree.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "vcd_types.h"
#include <stdlib.h>

/* ================================================= MACROS ================================================ */
#ifndef VCD_MALLOC
#define VCD_MALLOC(size)       malloc(size)
#define VCD_REALLOC(ptr, size) realloc((ptr), (size))
#define VCD_FREE(ptr)          free(ptr)
#endif

/* Longest identifier code produced by vcdEncodeId() (5 chars for 2^32 signals) plus NUL. */
#define VCD_ID_MAX_LEN 8u

#if defined(__GNUC__)
#define VCD_PRINTF_FMT(fmtIdx, argIdx) __attribute__((format(printf, fmtIdx, argIdx)))
#else
#define VCD_PRINTF_FMT(fmtIdx, argIdx)
#endif

/* =============================================== MODULE API ============================================== */

/* Writes the identifier code of signal number 'index' into buf (bijective base 94 over '!'..'~'),
 * so 0 -> "!", 93 -> "~", 94 -> "!!". Returns its length. buf must hold VCD_ID_MAX_LEN bytes. */
size_t vcdEncodeId(uint32_t index, char *buf);

/* Makes sure an array has room for 'need' elements, doubling its capacity as required.
 * Returns the (possibly moved) array, or NULL on failure, in which case the old array is untouched. */
void *vcdGrow(void *array, size_t *cap, size_t need, size_t elemSize);

/* malloc'd copies; NULL on allocation failure. */
char *vcdStrDup(const char *s);
char *vcdStrNDup(const char *s, size_t n);

#endif /* VCD_INTERNAL_H */
