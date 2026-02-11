/*
 * ThreadX Cortex-M interrupt dispatch for Thread-Metric.
 *
 * tm_cause_interrupt() executes SVC #0.  The hardware traps into
 * SVC_Handler which calls the benchmark interrupt handlers.
 *
 * On Cortex-M3, the hardware automatically saves/restores
 * {r0-r3, r12, lr, pc, xPSR} on exception entry/exit, providing
 * full context save/restore for fair measurement.
 *
 * The modern ThreadX Cortex-M3 port does NOT use the legacy
 * _tx_thread_context_save / _tx_thread_context_restore functions
 * (both are no-ops on this architecture).  Instead, ISRs are plain
 * C functions.  If a handler wakes a higher-priority thread (e.g.
 * tx_thread_resume), ThreadX pends PendSV internally.  PendSV fires
 * at the lowest exception priority after the SVC handler returns and
 * performs the actual context switch -- giving correct ISR-to-thread
 * preemption behavior.
 *
 * Both handlers have weak no-op defaults here.  The strong definition
 * from whichever interrupt test is linked overrides the no-op.
 */

#include "tm_api.h"

__attribute__((weak)) void tm_interrupt_handler(void) {}
__attribute__((weak)) void tm_interrupt_preemption_handler(void) {}

void SVC_Handler(void)
{
    tm_interrupt_handler();
    tm_interrupt_preemption_handler();
}

void tm_cause_interrupt(void)
{
    __asm volatile("svc #0");
}
