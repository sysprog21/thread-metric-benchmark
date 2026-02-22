/*
 * FreeRTOS Cortex-M interrupt dispatch for Thread-Metric.
 *
 * Cannot use SVC -- the FreeRTOS ARM_CM3 port uses SVC #0 for
 * starting the first task (vPortSVCHandler).  Instead, we use a
 * software-triggered NVIC external interrupt (IRQ 31) via the
 * Interrupt Set-Pending Register (ISPR).
 *
 * tm_cause_interrupt() pends IRQ 31 by writing to NVIC_ISPR0.
 * IRQ31_Handler (overriding the weak alias in vector_table.c)
 * dispatches to the benchmark interrupt handlers.
 *
 * tm_isr_dispatch_init() sets IRQ 31 priority and enables it in
 * the NVIC.  Called from tm_initialize() before the scheduler starts.
 *
 * Both handlers have weak no-op defaults here.  The strong definition
 * from whichever interrupt test is linked overrides the no-op.
 */

#include "tm_api.h"

/* NVIC register addresses (Cortex-M3) */
#define NVIC_ISER0 (*(volatile unsigned long *) 0xE000E100UL)
#define NVIC_ISPR0 (*(volatile unsigned long *) 0xE000E200UL)
#define NVIC_IPR7 (*(volatile unsigned long *) 0xE000E41CUL)

/* IRQ number used for software interrupt dispatch. */
#define TM_ISR_IRQ 31

__attribute__((weak)) void tm_interrupt_handler(void) {}
__attribute__((weak)) void tm_interrupt_preemption_handler(void) {}

/*
 * Override the weak IRQ31_Handler from vector_table.c.
 * Called by hardware when IRQ 31 is pended.
 */
void IRQ31_Handler(void)
{
    tm_interrupt_handler();
    tm_interrupt_preemption_handler();
}

/*
 * Initialize IRQ 31 in the NVIC: set priority and enable.
 * Priority is set to the lowest level (all priority bits set)
 * so that PendSV (used by FreeRTOS for context switch) can
 * still fire at its own priority.
 */
void tm_isr_dispatch_init(void)
{
    /* Set IRQ 31 priority.  IRQ 31 is in NVIC_IPR7 byte 3
     * (IPR offset = IRQ / 4 = 7, byte = IRQ % 4 = 3).
     * Set to 0xE0 (priority 7 << 5 for 3 priority bits) --
     * lowest priority, same as PendSV.
     */
    NVIC_IPR7 = (NVIC_IPR7 & 0x00FFFFFFUL) | (0xE0UL << 24);

    /* Enable IRQ 31 in NVIC. */
    NVIC_ISER0 = (1UL << TM_ISR_IRQ);
}

/*
 * Trigger a software interrupt by pending IRQ 31.
 * The NVIC will fire IRQ31_Handler as soon as it is the
 * highest-priority pending exception.
 */
void tm_cause_interrupt(void)
{
    NVIC_ISPR0 = (1UL << TM_ISR_IRQ);
}
