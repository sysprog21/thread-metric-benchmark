/*
 * ThreadX low-level initialization for QEMU mps2-an385 (Cortex-M3).
 *
 * Adapted from threadx/ports/cortex_m3/gnu/example_build/tx_initialize_low_level.S
 * for the mps2-an385 memory map and our shared vector_table.c / linker script.
 *
 * Responsibilities:
 *   1. Set _tx_initialize_unused_memory to first free RAM (_end from linker).
 *   2. Set VTOR to our vector table (vector_table from vector_table.c).
 *   3. Set _tx_thread_system_stack_ptr from vector table entry 0 (_estack).
 *   4. Configure SysTick for 100 Hz tick (ThreadX default).
 *   5. Set PendSV and SVC to lowest exception priority (0xFF).
 *   6. Provide SysTick_Handler that calls _tx_timer_interrupt.
 */

    .syntax unified
    .cpu    cortex-m3
    .thumb

    .global _tx_thread_system_stack_ptr
    .global _tx_initialize_unused_memory
    .global _tx_timer_interrupt

/* MPS2 AN385: 25 MHz system clock. */
SYSTEM_CLOCK   = 25000000
SYSTICK_CYCLES = ((SYSTEM_CLOCK / 100) - 1)

/* ------------------------------------------------------------------ */
/*  _tx_initialize_low_level                                          */
/* ------------------------------------------------------------------ */
    .section .text._tx_initialize_low_level
    .global  _tx_initialize_low_level
    .type    _tx_initialize_low_level, %function
    .thumb_func
_tx_initialize_low_level:
    /* Disable interrupts during ThreadX initialization. */
    cpsid   i

    /* Set first unused memory pointer (_end comes from linker script). */
    ldr     r0, =_tx_initialize_unused_memory
    ldr     r1, =_end
    adds    r1, r1, #4
    str     r1, [r0]

    /* Set VTOR to our vector table (vector_table from vector_table.c). */
    ldr     r0, =0xE000ED08
    ldr     r1, =vector_table
    str     r1, [r0]

    /* Set system stack pointer from vector table entry 0. */
    ldr     r0, =_tx_thread_system_stack_ptr
    ldr     r1, =vector_table
    ldr     r1, [r1]
    str     r1, [r0]

    /* Enable DWT cycle counter (available on Cortex-M3). */
    ldr     r0, =0xE0001000
    ldr     r1, [r0]
    orrs    r1, r1, #1
    str     r1, [r0]

    /* Configure SysTick: reload value, enable with processor clock + interrupt. */
    ldr     r0, =0xE000E000
    ldr     r1, =SYSTICK_CYCLES
    str     r1, [r0, #0x14]        /* SysTick Reload Value Register */
    movs    r1, #7                 /* CLKSOURCE | TICKINT | ENABLE  */
    str     r1, [r0, #0x10]        /* SysTick Control and Status    */

    /* Set handler priorities.
       PendSV and SVC must be at the lowest priority (0xFF) for correct
       ThreadX context switching.  SysTick at 0x40 (above default). */
    ldr     r0, =0xE000E000
    ldr     r1, =0x00000000
    str     r1, [r0, #0xD18]       /* System Handlers 4-7  (MemM, BusF, UsgF, Rsrv) */
    ldr     r1, =0xFF000000
    str     r1, [r0, #0xD1C]       /* System Handlers 8-11 (Rsrv, Rsrv, Rsrv, SVCall) */
    ldr     r1, =0x40FF0000
    str     r1, [r0, #0xD20]       /* System Handlers 12-15 (DbgM, Rsrv, PendSV, SysTick) */

    bx      lr
    .size _tx_initialize_low_level, . - _tx_initialize_low_level


/* ------------------------------------------------------------------ */
/*  SysTick_Handler -- overrides the weak alias in vector_table.c     */
/* ------------------------------------------------------------------ */
    .section .text.SysTick_Handler
    .global  SysTick_Handler
    .type    SysTick_Handler, %function
    .thumb_func
SysTick_Handler:
    push    {r0, lr}
    bl      _tx_timer_interrupt
    pop     {r0, lr}
    bx      lr
    .size SysTick_Handler, . - SysTick_Handler
