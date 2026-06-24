#ifndef TRACE_H
#define TRACE_H

/* ================================================ INCLUDES =============================================== */
#include <stdint.h>

/* ================================================= MACROS ================================================ */
/*
 * Lock-free in-memory ring buffer trace.
 *
 * Each slot is 8 bytes:
 *   [0]   tag   – (uart_id << 4) | op_tag
 *   [1]   data  – byte value or ms value
 *   [2..3]      – reserved / padding
 *   [4..7] cyc  – full 32-bit DWT->CYCCNT snapshot
 *
 * At 84 MHz the 32-bit counter wraps every ~51 seconds, which is more
 * than enough for any inter-message gap. DELTA in the decoder is now
 * always accurate regardless of timeout length.
 *
 * Writing is 5 instructions on CM4 (no branches, no locks).
 *
 * --- Reading the buffer from GDB ---
 *
 *   (gdb) source /home/rudy/Projects/libOBD2/scripts/dump_trace.py
 *   (gdb) dump_trace [N]
 */

#define TRACE_BUF_ENTRIES  512U   /* must be power of 2 */

/* Tags – lower nibble. Add your own as needed */
#define TRACE_TAG_TX          0x01   /* byte sent to bus              */
#define TRACE_TAG_RX          0x02   /* byte received                 */
#define TRACE_TAG_FLUSH       0x03   /* RX flush called               */
#define TRACE_TAG_TIMEOUT     0x04   /* timeout event                 */
#define TRACE_TAG_MARK        0x0F   /* generic marker                */

/* Timer-specific tags (UART field unused; set to 0 for timer events) */
#define TRACE_TAG_TMR_START   0x05   /* StartTimeout(ms) called       */
#define TRACE_TAG_TMR_STOP    0x06   /* StopTimeout called            */
#define TRACE_TAG_TMR_EXP_MAX 0x07   /* timeout expired  (MAX breach) */
#define TRACE_TAG_TMR_EXP_MIN 0x08   /* elapsed < timeMin (MIN breach)*/
#define TRACE_TAG_TMR_OK      0x09   /* byte arrived within window    */

/*
 * TRACE_TIMER_LOG(tag, ms_val)
 * Same cost as TRACE_LOG. ms_val is clamped to uint8_t (0-255 ms range
 * covers all P1/P2/P3/P4 constants). Use the data field to carry the
 * timeout value or elapsed time for post-mortem analysis.
 */
#define TRACE_TIMER_LOG(tag, ms_val)  trace_log((tag), (uint8_t)(ms_val), 0U)

/* UART ID is encoded in the upper nibble of the tag byte:
 *   tag = (uart_id << 4) | op_tag
 * uart_id is derived from the peripheral base address (libopencm3 values). */
static inline uint8_t trace_uart_id(uint32_t usart_base)
{
    switch (usart_base)
    {
        case 0x40011000UL: return 1; /* USART1 */
        case 0x40004400UL: return 2; /* USART2 */
        case 0x40004800UL: return 3; /* USART3 */
        case 0x40004C00UL: return 4; /* UART4  */
        case 0x40005000UL: return 5; /* UART5  */
        case 0x40011400UL: return 6; /* USART6 */
        default:           return 0;
    }
}

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    uint8_t  tag;
    uint8_t  data;
    uint16_t _pad;   /* reserved */
    uint32_t cyc;    /* DWT->CYCCNT full 32-bit snapshot */
} trace_entry_t;

/* ============================================ INLINE FUNCTIONS =========================================== */

/* DWT cycle counter – enabled once by trace_init() */
#define TRACE_DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004)
#define TRACE_DWT_CTRL    (*(volatile uint32_t *)0xE0001000)
#define TRACE_DEMCR       (*(volatile uint32_t *)0xE000EDFC)

extern volatile trace_entry_t g_trace_buf[TRACE_BUF_ENTRIES];
extern volatile uint32_t      g_trace_head; /* next write index, never masked externally */

/* Enable DWT cycle counter. Call once at startup (before any TRACE_LOG). */
static inline void trace_init(void)
{
    TRACE_DEMCR    |= (1U << 24); /* enable DWT */
    TRACE_DWT_CYCCNT = 0;
    TRACE_DWT_CTRL |= (1U << 0);  /* CYCCNTENA */
}

/*
 * TRACE_LOG(tag, byte)
 * ~4 CPU cycles. Safe anywhere – no malloc, no lock, no I/O.
 */
static inline void trace_log(uint8_t tag, uint8_t data, uint32_t uart_base)
{
    uint32_t idx = g_trace_head & (TRACE_BUF_ENTRIES - 1U);
    g_trace_buf[idx].tag  = (uint8_t)((trace_uart_id(uart_base) << 4) | (tag & 0x0FU));
    g_trace_buf[idx].data = data;
    g_trace_buf[idx]._pad = 0;
    g_trace_buf[idx].cyc  = TRACE_DWT_CYCCNT; /* full 32-bit */
    g_trace_head++;
}

#define TRACE_LOG(tag, byte, uart_base)  trace_log((tag), (uint8_t)(byte), (uint32_t)(uart_base))

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */

#endif /* TRACE_H */
