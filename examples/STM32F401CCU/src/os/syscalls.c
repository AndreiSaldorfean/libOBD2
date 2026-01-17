#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#define STM32F4
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/usart.h"

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
    for (i = 0; i < len; i++) {
      if (ptr[i] == '\n') {
        usart_send_blocking(USART1, '\r');
      }
      usart_send_blocking(USART1, ptr[i]);
    }
    return i;
  }
  errno = EIO;
  return -1;
}

int _read(int file, char *ptr, int len) {
  if (file != STDIN_FILENO) {
    errno = EBADF;
    return -1;
  }

  int i = 0;
  while (i < len) {
    ptr[i++] = usart_recv_blocking(USART1);
  }
  return i;
}
