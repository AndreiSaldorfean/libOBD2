#define STM32F4
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
  gpio_toggle(GPIOD, GPIO12);
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
void HardFault_Handler(void)
{
    uint32_t cont = 0;

    (void)cont;

    while (cont == 0);
}

// Default empty handler
void Default_Handler(void) {
  while (1) {
  }
}
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void Reset_Handler(void);

/* USB OTG FS interrupt handler for TinyUSB */
void otg_fs_isr(void)
{
    tud_int_handler(0);
}

/* TIM2 interrupt handler - defined in kwp_timer.c */
extern void tim2_isr(void);

__attribute__((section(".isr_vector"))) void (*const vector_table[])(void) = {
    (void (*)(void))(0x20000000 + 128 * 1024), // Initial stack pointer
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
    // External interrupts start here (IRQ 0-66)
    [16 ... 43] = Default_Handler,  // IRQ 0-27 default
    tim2_isr,                       // IRQ 28: TIM2
    [45 ... 82] = Default_Handler,  // IRQ 29-66 default
    otg_fs_isr,        // IRQ 67: USB OTG FS
    [84 ... 100] = Default_Handler, // Rest default
};
