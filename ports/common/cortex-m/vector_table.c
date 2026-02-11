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
};

void Default_Handler(void)
{
    for (;;)
        ;
}
