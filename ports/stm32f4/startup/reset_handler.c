#include <stdint.h>

extern int main();

extern uint32_t _etext, _sdata, _edata, __bss_start__, __bss_end__;

void Reset_Handler(void) {
  uint32_t *src = &_etext;
  uint32_t *dst = &_sdata;
  while (dst < &_edata)
    *dst++ = *src++;

  // Zero .bss
  dst = &__bss_start__;
  while (dst < &__bss_end__)
    *dst++ = 0;
  // Call main
  main();

  // If main returns, loop forever
  while (1)
    ;
}
