/*
 * ARM semihosting primitives for Cortex-M QEMU builds.
 *
 * RTOS-neutral -- shared by all Cortex-M porting layers.
 * Compiled only when TM_SEMIHOSTING is defined.
 *
 * These bypass newlib entirely.  In particular, tm_semihosting_exit()
 * avoids the deep call chain in newlib's _exit() which probes for
 * SYS_EXIT_EXTENDED support by opening ":semihosting-features",
 * calling __sinit (stdio init that depends on initialise_monitor_handles,
 * which -nostartfiles omits), and performing multiple file I/O
 * semihosting operations.  On some toolchain/QEMU combinations this
 * chain faults or hangs because the newlib reent structure was never
 * properly initialized.
 */

#include "tm_api.h"

void tm_putchar(int c)
{
    /* ARM semihosting SYS_WRITEC (0x03). */
    char ch = (char) c;
    register int r0 __asm__("r0") = 0x03;
    register char *r1 __asm__("r1") = &ch;
    __asm__ volatile("bkpt #0xAB" : : "r"(r0), "r"(r1) : "memory");
}

void tm_semihosting_exit(int code)
{
    /* ARM semihosting SYS_EXIT (0x18).
     * On M-profile, r1 is the reason code directly (not a pointer).
     * ADP_Stopped_ApplicationExit = 0x20026.
     */
    register int r0 __asm__("r0") = 0x18;
    register int r1 __asm__("r1") = code == 0 ? 0x20026 : 0x20024;
    __asm__ volatile("bkpt #0xAB" : : "r"(r0), "r"(r1) : "memory");
    for (;;)
        ;
}
