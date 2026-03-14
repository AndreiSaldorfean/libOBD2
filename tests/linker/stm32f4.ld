ENTRY(Reset_Handler)

MEMORY
{
  flash (rx): ORIGIN = 0x08000000, LENGTH = 1024K
  ram   (rwx): ORIGIN = 0x20000000, LENGTH = 128K
  ccm   (rwx): ORIGIN = 0x10000000, LENGTH = 64K
}

__stack_size__ = 16K;

SECTIONS
{
  . = ORIGIN(flash);

  .isr_vector : {
    KEEP(*(.isr_vector))
  } >flash

  .text : {
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
    *(.ARM.exidx*)
    _etext = .;
  } >flash

  .data : AT(_etext) {
    _sdata = .;
    *(.data*)
    _edata = .;
  } >ram

  .bss : {
    __bss_start__ = .;
    *(.bss*)
    *(COMMON)
    __bss_end__ = .;
  } >ram

  .init_array : {
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
  } >ram

  .fini_array : {
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array))
  } >ram

  /* Heap comes after BSS and before stack */
  .heap (NOLOAD) : ALIGN(8) {
    PROVIDE(end = .);          /* newlib-compatible heap start */
    PROVIDE(_heap_start = .);
    . = . + 0x800;             /* 2 KB heap */
    PROVIDE(_heap_end = .);
  } >ram

  .stack (NOLOAD) : ALIGN(8) {
    __stack_start__ = .;
    . = . + __stack_size__;
    __stack_end__ = .;
  } >ram

  _end = .;
}

/* Symbol aliases for C code */
__heap_start__ = _heap_start;
__heap_end__   = _heap_end;
