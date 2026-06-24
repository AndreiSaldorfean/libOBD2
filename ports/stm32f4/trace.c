/* ================================================ INCLUDES =============================================== */
#include "trace.h"

/* ============================================ GLOBAL VARIABLES =========================================== */
volatile trace_entry_t g_trace_buf[TRACE_BUF_ENTRIES];
volatile uint32_t      g_trace_head = 0;
