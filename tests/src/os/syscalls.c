#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#define STM32F4
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"

#include "tusb.h"
#include "device/usbd.h"
#include "class/cdc/cdc_device.h"

extern uint32_t __heap_end__; // heap starts here
extern uint32_t _heap_start;
extern uint32_t __stack_start__; // stack starts here (stack grows down)
static uint32_t *heap = NULL;

extern int *__errno(void);

void _exit(int status) {
  (void)status;
  for (;;)
    ;
}

int _close(int file) { return -1; }

int _lseek(int file, int ptr, int dir) { return 0; }

int _fstat(int file, struct stat *st) { return 0; }

int _isatty(int file) { return 1; }
int _kill() {}
int _getpid() {}

void *_sbrk(ptrdiff_t incr) {
  if (heap == NULL)
    heap = &_heap_start;

  uint32_t *prev_heap = heap;
  uint32_t *next_heap = heap + incr;

  // Prevent heap-stack collision
  if (next_heap > (uint8_t *)&__heap_end__) {
    errno = ENOMEM;
    return (void *)-1;
  }

  heap = next_heap;
  return (void *)prev_heap;
}

int _write(int file, char *ptr, int len) {
  int i;

  if (file == STDOUT_FILENO || file == STDERR_FILENO) {
     /* Wait for USB CDC to connect if not already connected */
     while (!tud_cdc_connected()) {
       tud_task();
     }

     /* Write data to CDC - convert \n to \r\n for proper terminal display */
     uint32_t written = 0;
     for (int i = 0; i < len; i++) {
       /* Wait for write buffer space to be available */
       while (tud_cdc_write_available() == 0) {
         tud_cdc_write_flush();
         tud_task();
       }

       /* If we encounter \n, send \r first */
       if (ptr[i] == '\n') {
         tud_cdc_write_char('\r');
       }
       tud_cdc_write_char(ptr[i]);
       written++;
     }

     /* Flush and give USB a bit of time to transmit */
     tud_cdc_write_flush();
     for (int i = 0; i < 10; i++) {
       tud_task();
     }

     return len;
  }
  errno = EIO;
  return -1;
}

int _read(int file, char *ptr, int len) {
  if (file != STDIN_FILENO) {
    errno = EBADF;
    return -1;
  }

  /* Read from TinyUSB CDC */
  if (tud_cdc_connected() && tud_cdc_available()) {
    uint32_t count = tud_cdc_read(ptr, len);
    return count;
  }

  return 0; // No data available
}

/* putchar implementation for Unity - Unity uses putchar() by default */
int putchar(int c) {
  char ch = (char)c;
  _write(STDOUT_FILENO, &ch, 1);
  return c;
}
