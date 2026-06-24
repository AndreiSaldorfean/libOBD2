#ifndef INIT_H
#define INIT_H

/* ================================================ INCLUDES =============================================== */
#define STM32F4
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/usart.h"
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

/* ================================================= MACROS ================================================ */
/* ARM semihosting SYS_WRITEC (op 0x03) – single char output over SWD.
 * Works with OpenOCD + ST-Link out of the box: output appears in the
 * OpenOCD console. Requires 'monitor arm semihosting enable' in GDB
 * (already added to gdbhooks.gdb). If no debugger is attached the BKPT
 * will fault, so guard with DEBUG if needed. */
static inline void dbg_putc(char c)
{
    register int       op  __asm__("r0") = 0x03; /* SYS_WRITEC */
    register const char *arg __asm__("r1") = &c;
    __asm__ volatile ("bkpt 0xAB" : : "r"(op), "r"(arg) : "memory");
}
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* ============================================ INLINE FUNCTIONS =========================================== */
/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */
/* =============================================== MODULE API ============================================== */
void sysInit(void);

#endif /* INIT_H */
