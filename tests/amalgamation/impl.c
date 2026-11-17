/* Smoke test for the single header: the one TU that holds the implementation. */
#define LIBOBD2_IMPLEMENTATION
#include "libobd2.h"
#include "libobd2.h" /* a second include must be harmless */
