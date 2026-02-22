/*
 * ARM semihosting character output for tm_printf().
 *
 * RTOS-neutral -- shared by all Cortex-M porting layers.
 * Compiled only for cortex-m-qemu builds (TM_SEMIHOSTING defined).
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
