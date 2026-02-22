/*
 * FreeRTOS configuration for Cortex-M3 on QEMU mps2-an385.
 *
 * MPS2 AN385: Cortex-M3, 25 MHz, 4 MB FLASH + 4 MB SSRAM.
 * FreeRTOS ARM_CM3 port uses PendSV for context switch, SVC for
 * first-task start, and SysTick for tick generation.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Hardware */
#define configCPU_CLOCK_HZ ((unsigned long) 25000000)

/* Scheduler */
#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TIME_SLICING 1

/* Sizing */
#define configMAX_PRIORITIES 32
#define configMINIMAL_STACK_SIZE ((unsigned short) 128)
#define configTOTAL_HEAP_SIZE ((size_t) 32768)
#define configMAX_TASK_NAME_LEN 16

/* Tick */
#define configTICK_RATE_HZ ((TickType_t) 1000)
#define configUSE_16_BIT_TICKS 0

/* Features */
#define configUSE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_QUEUE_SETS 0

/* Hooks */
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 0

/* Timer daemon */
#define configUSE_TIMERS 0
#define configTIMER_TASK_PRIORITY 2
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH configMINIMAL_STACK_SIZE

/* Co-routines */
#define configUSE_CO_ROUTINES 0
#define configMAX_CO_ROUTINE_PRIORITIES 1

/* INCLUDE functions needed by Thread-Metric */
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_xTaskResumeFromISR 1

/* Assert -- disabled for benchmark */
#define configASSERT(x)

/* Trace */
#define configUSE_TRACE_FACILITY 0

/*
 * Cortex-M3 NVIC configuration.
 *
 * QEMU mps2-an385 Cortex-M3 implements 3 priority bits (8 levels).
 * FreeRTOS needs configPRIO_BITS to compute BASEPRI masks.
 */
#define configPRIO_BITS 3

/* Lowest interrupt priority (all bits set). */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 7

/* Maximum priority from which FreeRTOS API calls can be made.
 * Interrupts at higher priority (lower number) must not call
 * FreeRTOS API functions.
 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/*
 * Map FreeRTOS handler names to the weak aliases in vector_table.c.
 * The ARM_CM3 port defines vPortSVCHandler, xPortPendSVHandler, and
 * xPortSysTickHandler -- redirect them to the standard CMSIS names.
 */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
