#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "class/cdc/cdc_device.h"
#include "device/usbd.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/rcc.h"
#include "tusb.h"
#include <sys/stat.h>
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "semphr.h"

extern uint32_t __heap_end__; // heap starts here
extern uint32_t _heap_start;
extern uint32_t __stack_start__; // stack starts here (stack grows down)
static uint32_t *heap = NULL;
extern SemaphoreHandle_t g_printMutex;

extern int *__errno(void);

void _exit(int status)
{
    (void)status;
    for (;;)
        ;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;

    return 0;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    (void)st;

    return 0;
}

int _isatty(int file)
{
    (void)file;

    return 1;
}

int _kill(void)
{
    return 0;
}

int _getpid(void)
{
    return 0;
}

void *_sbrk(ptrdiff_t incr)
{
    if (heap == NULL)
        heap = &_heap_start;

    uint32_t *prev_heap = heap;
    uint32_t *next_heap = heap + incr;

    // Prevent heap-stack collision
    if (next_heap > &__heap_end__)
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap = next_heap;
    return (void *)prev_heap;
}

#if 0
int _write(int file, char *ptr, uint32_t len)
{
    (void)file;
    (void)ptr;
    (void)len;

    #if !defined(DEBUG)
    xSemaphoreTake(g_printMutex, portMAX_DELAY);
    if (file == STDOUT_FILENO || file == STDERR_FILENO)
    {
        /* Wait for USB CDC to connect if not already connected */
        while (!tud_cdc_connected())
        {
            tud_task();
        }

        /* Write data to CDC, converting \n to \r\n */
        for (uint32_t i = 0; i < len; i++)
        {
            /* Wait for write buffer space */
            while (tud_cdc_write_available() == 0)
            {
                tud_cdc_write_flush();
                tud_task();
            }

            /* Convert LF to CRLF for proper terminal display */
            if (ptr[i] == '\n')
            {
                tud_cdc_write_char('\r');
            }
            tud_cdc_write_char(ptr[i]);
        }

        /* Flush and give USB a bit of time to transmit */
        tud_cdc_write_flush();
        for (int i = 0; i < 10; i++)
        {
            tud_task();
        }

        xSemaphoreGive(g_printMutex);
        return len;
    }

    errno = EIO;
    xSemaphoreGive(g_printMutex);
    #endif
    return -1;
}
#endif

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;

    #if !defined(DEBUG)
    if (file != STDIN_FILENO)
    {
        errno = EBADF;
        return -1;
    }

    /* Read from TinyUSB CDC */
    if (tud_cdc_connected() && tud_cdc_available())
    {
        uint32_t count = tud_cdc_read(ptr, len);
        return count;
    }
    #endif

    return 0; // No data available
}

/* Required by __libc_init_array called from Reset_Handler */
void _init(void) {}
