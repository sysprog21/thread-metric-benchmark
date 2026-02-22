/*
 * FreeRTOS configuration for POSIX (Linux/macOS) simulator.
 *
 * The FreeRTOS POSIX port (portable/ThirdParty/GCC/Posix/) maps each
 * task to a pthread and uses signals for context switching.  This
 * config sets the kernel parameters for Thread-Metric benchmarking.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Scheduler */
#define configUSE_PREEMPTION            1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TIME_SLICING          1

/* Sizing */
#define configMAX_PRIORITIES            32
#define configMINIMAL_STACK_SIZE        ((unsigned short) 256)
#define configTOTAL_HEAP_SIZE           ((size_t) 65536)
#define configMAX_TASK_NAME_LEN         16

/* Tick */
#define configTICK_RATE_HZ              ((TickType_t) 1000)
#define configUSE_16_BIT_TICKS          0

/* Features used by Thread-Metric */
#define configUSE_MUTEXES               0
#define configUSE_COUNTING_SEMAPHORES   0
#define configUSE_RECURSIVE_MUTEXES     0
#define configUSE_QUEUE_SETS            0

/* Hooks -- not needed for benchmarking */
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configUSE_MALLOC_FAILED_HOOK    0
#define configCHECK_FOR_STACK_OVERFLOW  0

/* Timer daemon -- not needed */
#define configUSE_TIMERS                0
#define configTIMER_TASK_PRIORITY       2
#define configTIMER_QUEUE_LENGTH        10
#define configTIMER_TASK_STACK_DEPTH    configMINIMAL_STACK_SIZE

/* Co-routines -- not used */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES 1

/* INCLUDE functions needed by Thread-Metric */
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_vTaskDelete             1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_uxTaskPriorityGet       1
#define INCLUDE_vTaskPrioritySet        1
#define INCLUDE_xTaskResumeFromISR  1

/* Assert -- disabled for benchmark (matches TX_DISABLE_ERROR_CHECKING) */
#define configASSERT(x)

/* Trace -- disabled */
#define configUSE_TRACE_FACILITY        0

/* POSIX port needs this for stack allocation */
#define configSTACK_DEPTH_TYPE          unsigned long

#endif /* FREERTOS_CONFIG_H */
