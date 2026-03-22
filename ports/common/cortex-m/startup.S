/*
 * Minimal Cortex-M3 reset handler for QEMU mps2-an385.
 *
 * The hardware loads SP from vector table entry 0 on reset, so the
 * stack is valid when Reset_Handler executes.  We copy .data from
 * FLASH to RAM, zero .bss, call SystemInit() (weak no-op by default),
 * and branch to main().
 *
 * Shared across all RTOS ports -- each RTOS only provides its kernel
 * library and tm_port.c.
 */

    .syntax unified
    .cpu    cortex-m3
    .thumb

/* Reset handler */

    .section .text.Reset_Handler
    .global  Reset_Handler
    .type    Reset_Handler, %function
Reset_Handler:
    /* Copy .data initializers from FLASH (LMA) to RAM (VMA). */
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
    b       .Lcopy_check
.Lcopy_loop:
    ldr     r3, [r2], #4
    str     r3, [r0], #4
.Lcopy_check:
    cmp     r0, r1
    blo     .Lcopy_loop

    /* Zero .bss. */
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r2, #0
    b       .Lzero_check
.Lzero_loop:
    str     r2, [r0], #4
.Lzero_check:
    cmp     r0, r1
    blo     .Lzero_loop

    /* Call SystemInit (weak default is a no-op). */
    bl      SystemInit

    /* Branch to main with argc=0, argv=NULL.
     * Bare-metal has no command line; rdimon does not populate
     * argc/argv from SYS_GET_CMDLINE on arm-none-eabi-gcc 15.x.
     * Leaving r0/r1 as-is after the .bss loop passes garbage.
     */
    movs    r0, #0
    movs    r1, #0
    bl      main
.Lhang:
    b       .Lhang
    .size   Reset_Handler, . - Reset_Handler

/* Default SystemInit -- no-op, override in board-specific code. */

    .section .text.SystemInit
    .weak   SystemInit
    .type   SystemInit, %function
SystemInit:
    bx      lr
    .size   SystemInit, . - SystemInit

/* _fini / _init stubs for newlib.
 * -nostartfiles skips crt0 which normally provides these.
 */

    .section .text._init
    .global  _init
    .type    _init, %function
_init:
    bx      lr
    .size   _init, . - _init

    .section .text._fini
    .global  _fini
    .type    _fini, %function
_fini:
    bx      lr
    .size   _fini, . - _fini
