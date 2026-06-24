#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "tusb.h"

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));

extern void xPortSysTickHandler(void);
// SysTick handler required by FreeRTOS
void SysTick_Handler(void) {
  xPortSysTickHandler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        // Optionally toggle an LED or log here
    }
}

void HardFault_Handler(void) {
    bool continue_ = 1;
    while (continue_);
}

// Default empty handler
void Default_Handler(void) {
  while (1) {
  }
}
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void Reset_Handler(void);

/* USB FS LP interrupt handler for TinyUSB (G4 uses USB FS, not OTG) */
void usb_lp_isr(void)
{
    tud_int_handler(0);
}

/* TIM2 interrupt handler - defined in libobd2_timer_port.c */
extern void tim2_isr(void);

/* NOTE: Renamed vector_table to obd2VectorTable due to collision with libopencm3 */
__attribute__((section(".isr_vector"))) void (*const isr_vector[])(void) = {
    (void (*)(void))(0x20000000 + 96 * 1024), // Initial stack pointer (96K SRAM1)
    Reset_Handler,                             // Reset handler
    NMI_Handler,                               // NMI handler
    HardFault_Handler,                         // Hard fault handler
    MemManage_Handler,                         // MPU fault handler
    BusFault_Handler,                          // Bus fault handler
    UsageFault_Handler,                        // Usage fault handler
    0, 0, 0, 0,        // Reserved
    vPortSVCHandler,   // SVCall handler
    DebugMon_Handler,  // Debug monitor handler
    0,                 // Reserved
    xPortPendSVHandler,// PendSV handler
    SysTick_Handler,   // SysTick handler (used by FreeRTOS)
    // External interrupts (IRQ0+)
    [16 ... 35] = Default_Handler,  // IRQ 0-19 default
    usb_lp_isr,                     // IRQ 20: USB LP
    [37 ... 43] = Default_Handler,  // IRQ 21-27 default
    tim2_isr,                       // IRQ 28: TIM2
    [45 ... 100] = Default_Handler, // Rest default
};
