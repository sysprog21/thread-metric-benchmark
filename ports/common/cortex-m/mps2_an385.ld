/*
 * Linker script for QEMU mps2-an385 (Cortex-M3).
 *
 * Memory map (QEMU emulation of ARM MPS2 FPGA Prototyping Board,
 * AN385 application note image):
 *
 *   FLASH: 0x00000000  4 MB  (ZBT SSRAM1, code region)
 *   RAM:   0x20000000  4 MB  (ZBT SSRAM2/3, data region)
 *
 * Shared across all RTOS ports targeting mps2-an385.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

/* Initial stack pointer -- top of RAM. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

ENTRY(Reset_Handler)

SECTIONS
{
    /* Vector table must be first in FLASH. */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* .data initializers stored in FLASH, copied to RAM by startup. */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Heap: grows up from _end.  _sbrk() in syscalls.c uses _end
       and _estack to bound allocation. */
    . = ALIGN(4);
    _end = .;
    PROVIDE(end = .);

    /* Stack: grows down from _estack (top of RAM).  The hardware
       loads _estack into SP on reset via vector table entry 0. */
}
