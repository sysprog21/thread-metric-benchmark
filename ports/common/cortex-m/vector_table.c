/*
 * Default Cortex-M3 vector table for QEMU mps2-an385.
 *
 * All exception handlers are weak aliases to Default_Handler (infinite
 * loop).  Each RTOS overrides the handlers it owns:
 *
 *   FreeRTOS:  SVC_Handler, PendSV_Handler, SysTick_Handler
 *   ThreadX:   PendSV_Handler, SysTick_Handler
 *   Zephyr:    uses its own vector table (not this file)
 *   RT-Thread: PendSV_Handler, SysTick_Handler, HardFault_Handler
 *
 * Placed at FLASH base (0x00000000) via the .isr_vector section in
 * the linker script.
 */

extern unsigned long _estack;
extern void Reset_Handler(void);

void Default_Handler(void);

/* Weak aliases -- override in RTOS port or application code. */
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

/* External interrupt handlers (IRQ 0-31).  Weak aliases allow any RTOS
 * port or application code to override individual handlers.  FreeRTOS
 * Cortex-M port uses IRQ 31 for software-triggered interrupt dispatch.
 */
void IRQ0_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ1_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ2_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ3_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ4_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ5_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ6_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ7_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ8_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ9_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ10_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ11_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ12_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ13_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ14_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ15_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ16_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ17_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ18_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ19_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ20_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ21_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ22_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ23_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ24_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ25_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ26_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ27_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ28_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ29_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ30_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ31_Handler(void) __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"),
               used)) void (*const vector_table[])(void) = {
    (void (*)(void))(&_estack), /*  0: Initial stack pointer         */
    Reset_Handler,              /*  1: Reset                         */
    NMI_Handler,                /*  2: NMI                           */
    HardFault_Handler,          /*  3: Hard fault                    */
    MemManage_Handler,          /*  4: MPU fault                     */
    BusFault_Handler,           /*  5: Bus fault                     */
    UsageFault_Handler,         /*  6: Usage fault                   */
    0,                          /*  7: Reserved                      */
    0,                          /*  8: Reserved                      */
    0,                          /*  9: Reserved                      */
    0,                          /* 10: Reserved                      */
    SVC_Handler,                /* 11: SVCall                        */
    DebugMon_Handler,           /* 12: Debug monitor                 */
    0,                          /* 13: Reserved                      */
    PendSV_Handler,             /* 14: PendSV                        */
    SysTick_Handler,            /* 15: SysTick                       */
    /* External interrupts (IRQ 0-31) */
    IRQ0_Handler,               /* 16: IRQ  0                        */
    IRQ1_Handler,               /* 17: IRQ  1                        */
    IRQ2_Handler,               /* 18: IRQ  2                        */
    IRQ3_Handler,               /* 19: IRQ  3                        */
    IRQ4_Handler,               /* 20: IRQ  4                        */
    IRQ5_Handler,               /* 21: IRQ  5                        */
    IRQ6_Handler,               /* 22: IRQ  6                        */
    IRQ7_Handler,               /* 23: IRQ  7                        */
    IRQ8_Handler,               /* 24: IRQ  8                        */
    IRQ9_Handler,               /* 25: IRQ  9                        */
    IRQ10_Handler,              /* 26: IRQ 10                        */
    IRQ11_Handler,              /* 27: IRQ 11                        */
    IRQ12_Handler,              /* 28: IRQ 12                        */
    IRQ13_Handler,              /* 29: IRQ 13                        */
    IRQ14_Handler,              /* 30: IRQ 14                        */
    IRQ15_Handler,              /* 31: IRQ 15                        */
    IRQ16_Handler,              /* 32: IRQ 16                        */
    IRQ17_Handler,              /* 33: IRQ 17                        */
    IRQ18_Handler,              /* 34: IRQ 18                        */
    IRQ19_Handler,              /* 35: IRQ 19                        */
    IRQ20_Handler,              /* 36: IRQ 20                        */
    IRQ21_Handler,              /* 37: IRQ 21                        */
    IRQ22_Handler,              /* 38: IRQ 22                        */
    IRQ23_Handler,              /* 39: IRQ 23                        */
    IRQ24_Handler,              /* 40: IRQ 24                        */
    IRQ25_Handler,              /* 41: IRQ 25                        */
    IRQ26_Handler,              /* 42: IRQ 26                        */
    IRQ27_Handler,              /* 43: IRQ 27                        */
    IRQ28_Handler,              /* 44: IRQ 28                        */
    IRQ29_Handler,              /* 45: IRQ 29                        */
    IRQ30_Handler,              /* 46: IRQ 30                        */
    IRQ31_Handler,              /* 47: IRQ 31                        */
};

void Default_Handler(void)
{
    for (;;)
        ;
}
