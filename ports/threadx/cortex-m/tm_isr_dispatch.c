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

#include <stdbool.h>
#include "tm_api.h"

__attribute__((weak)) void tm_interrupt_handler(void) {}
__attribute__((weak)) void tm_interrupt_preemption_handler(void) {}

extern void tm_threadx_benchmark_sync_complete(void);

static volatile bool tm_benchmark_interrupt_active;

void SVC_Handler(void)
{
    tm_interrupt_handler();
    tm_interrupt_preemption_handler();
}

void tm_cause_interrupt(void)
{
    __asm volatile("svc #0");
}

bool tm_benchmark_interrupt_context_active(void)
{
    return tm_benchmark_interrupt_active;
}

/* Synchronous variant: skip the SVC trap and run the handler in-line on
 * the caller's stack while letting the port wrappers bracket each RTOS
 * service with synthetic ISR context.
 */
void tm_cause_interrupt_sync(void)
{
    __asm volatile("cpsid i" ::: "memory");
    tm_benchmark_interrupt_active = true;
    tm_interrupt_handler();
    tm_benchmark_interrupt_active = false;
    __asm volatile("cpsie i" ::: "memory");
    tm_threadx_benchmark_sync_complete();
}
