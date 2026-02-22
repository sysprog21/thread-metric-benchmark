/*
 * FreeRTOS porting layer for Thread-Metric benchmarks.
 *
 * Implements the 14 functions declared in tm_api.h against the
 * FreeRTOS kernel.  Works on both the POSIX simulator and real
 * Cortex-M hardware (QEMU mps2-an385).
 *
 * Key design decisions vs. the external reference:
 *   - Priority mapping: (configMAX_PRIORITIES - 1) - tm_priority
 *     TM 1 (highest) -> FreeRTOS 30, TM 31 (lowest) -> 0.
 *   - Queue message size: 4 * sizeof(unsigned long), not hardcoded 16.
 *     Fixes LP64 breakage (32-byte messages on 64-bit hosts).
 *   - Memory pool: O(1) freelist of 128-byte blocks in static buffer.
 *     Fair comparison with ThreadX tx_block_* (not pvPortMalloc).
 *   - Tasks created suspended: xTaskCreate + vTaskSuspend before
 *     scheduler starts (matches ThreadX TX_DONT_START).
 *   - ISR simulation (POSIX): highest-priority task blocks on binary
 *     semaphore.  tm_cause_interrupt() gives it -> immediate preemption.
 *   - ISR-safe Cortex-M: uses xTaskResumeFromISR / xSemaphoreGiveFromISR
 *     when called from interrupt context.
 */

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <stdio.h>
#include <task.h>
#include "tm_api.h"


/* Constants */

#define TM_FREERTOS_MAX_THREADS 10
#define TM_FREERTOS_MAX_QUEUES 1
#define TM_FREERTOS_MAX_SEMAPHORES 1
#define TM_FREERTOS_MAX_POOLS 1

#define TM_FREERTOS_STACK_DEPTH 512 /* words (configSTACK_DEPTH_TYPE) */
#define TM_FREERTOS_QUEUE_DEPTH 10
#define TM_FREERTOS_QUEUE_MSG_SIZE (4 * sizeof(unsigned long))

/* Memory pool: 128-byte blocks, 2048-byte buffer -> 16 blocks. */
#define TM_BLOCK_SIZE 128
#define TM_POOL_SIZE 2048
#define TM_BLOCK_COUNT (TM_POOL_SIZE / TM_BLOCK_SIZE)


/* Data structures */

static TaskHandle_t tm_thread_array[TM_FREERTOS_MAX_THREADS];
static QueueHandle_t tm_queue_array[TM_FREERTOS_MAX_QUEUES];
static SemaphoreHandle_t tm_semaphore_array[TM_FREERTOS_MAX_SEMAPHORES];

/* Entry function table + trampoline (FreeRTOS task signature differs). */
static void (*tm_thread_entry_functions[TM_FREERTOS_MAX_THREADS])(void);

static void tm_task_trampoline(void *param)
{
    int id = (int) (unsigned long) param;
    tm_thread_entry_functions[id]();
    /* Benchmark threads loop forever, but guard against accidental
     * return -- FreeRTOS tasks must not fall off the end.
     */
    vTaskDelete(NULL);
}


/* O(1) fixed-block memory pool (no kernel involvement) */

/* Pool storage -- aligned for any basic type. */
static unsigned char tm_pool_area[TM_FREERTOS_MAX_POOLS][TM_POOL_SIZE]
    __attribute__((aligned(sizeof(void *))));

/* Freelist head per pool.  Each free block stores a pointer to the
 * next free block in its first sizeof(void*) bytes.
 */
static void *tm_pool_free[TM_FREERTOS_MAX_POOLS];


/* ISR simulation -- POSIX host */

#ifdef TM_ISR_VIA_THREAD

static TaskHandle_t tm_isr_task;
static SemaphoreHandle_t tm_isr_sem;

__attribute__((weak)) void tm_interrupt_handler(void) {}
__attribute__((weak)) void tm_interrupt_preemption_handler(void) {}

static void tm_isr_task_entry(void *param)
{
    (void) param;
    for (;;) {
        xSemaphoreTake(tm_isr_sem, portMAX_DELAY);
        tm_interrupt_handler();
        tm_interrupt_preemption_handler();
    }
}

void tm_cause_interrupt(void)
{
    xSemaphoreGive(tm_isr_sem);
}

#endif /* TM_ISR_VIA_THREAD */


/* Cortex-M ISR dispatch (provided by tm_isr_dispatch.c) */

#if defined(__arm__) && !defined(TM_ISR_VIA_THREAD)
extern void tm_isr_dispatch_init(void);
/* tm_cause_interrupt() defined in tm_isr_dispatch.c */
#endif


/* tm_initialize */

void tm_initialize(void (*test_initialization_function)(void))
{
#ifdef TM_ISR_VIA_THREAD
    /* ISR simulation thread at highest FreeRTOS priority.
     * Created before the test threads so it exists when
     * tm_cause_interrupt() is called.
     */
    tm_isr_sem = xSemaphoreCreateBinary();
    if (tm_isr_sem == NULL ||
        xTaskCreate(tm_isr_task_entry, "ISR", TM_FREERTOS_STACK_DEPTH, NULL,
                    configMAX_PRIORITIES - 1, &tm_isr_task) != pdPASS) {
        tm_check_fail("FATAL: ISR simulation setup failed\n");
    }
#endif

    /* Let the test create its threads. */
    test_initialization_function();

#if defined(__arm__) && !defined(TM_ISR_VIA_THREAD)
    tm_isr_dispatch_init();
#endif

    /* Start the scheduler -- does not return. */
    vTaskStartScheduler();
}


/* Thread management */

int tm_thread_create(int thread_id, int priority, void (*entry_function)(void))
{
    BaseType_t status;
    UBaseType_t freertos_prio;

    /* Invert priority: TM 1 (highest) -> configMAX_PRIORITIES-2,
     * TM 31 (lowest) -> 0.  Reserve configMAX_PRIORITIES-1 for ISR.
     */
    freertos_prio = (UBaseType_t) ((configMAX_PRIORITIES - 1) - priority);

    tm_thread_entry_functions[thread_id] = entry_function;

    status = xTaskCreate(tm_task_trampoline, "TM", TM_FREERTOS_STACK_DEPTH,
                         (void *) (unsigned long) thread_id, freertos_prio,
                         &tm_thread_array[thread_id]);

    if (status != pdPASS)
        return TM_ERROR;

    /* Create in suspended state (matches ThreadX TX_DONT_START).
     * Safe because the scheduler has not started yet.
     */
    vTaskSuspend(tm_thread_array[thread_id]);

    return TM_SUCCESS;
}

int tm_thread_resume(int thread_id)
{
#if defined(__arm__) && !defined(TM_ISR_VIA_THREAD)
    /* Cortex-M: detect ISR context for ISR-safe resume. */
    if (xPortIsInsideInterrupt()) {
        BaseType_t yield = pdFALSE;
        yield = xTaskResumeFromISR(tm_thread_array[thread_id]);
        portYIELD_FROM_ISR(yield);
        return TM_SUCCESS;
    }
#endif
    vTaskResume(tm_thread_array[thread_id]);
    return TM_SUCCESS;
}

int tm_thread_suspend(int thread_id)
{
    vTaskSuspend(tm_thread_array[thread_id]);
    return TM_SUCCESS;
}

void tm_thread_relinquish(void)
{
    taskYIELD();
}

void tm_thread_sleep(int seconds)
{
    vTaskDelay(pdMS_TO_TICKS(seconds * 1000U));
}


/* Queue management */

int tm_queue_create(int queue_id)
{
    tm_queue_array[queue_id] =
        xQueueCreate(TM_FREERTOS_QUEUE_DEPTH, TM_FREERTOS_QUEUE_MSG_SIZE);

    if (tm_queue_array[queue_id] == NULL)
        return TM_ERROR;

    return TM_SUCCESS;
}

int tm_queue_send(int queue_id, unsigned long *message_ptr)
{
    if (xQueueSendToBack(tm_queue_array[queue_id], (const void *) message_ptr,
                         0) != pdTRUE)
        return TM_ERROR;

    return TM_SUCCESS;
}

int tm_queue_receive(int queue_id, unsigned long *message_ptr)
{
    if (xQueueReceive(tm_queue_array[queue_id], (void *) message_ptr, 0) !=
        pdTRUE)
        return TM_ERROR;

    return TM_SUCCESS;
}


/* Semaphore management */

int tm_semaphore_create(int semaphore_id)
{
    tm_semaphore_array[semaphore_id] = xSemaphoreCreateBinary();

    if (tm_semaphore_array[semaphore_id] == NULL)
        return TM_ERROR;

    /* Start available (count = 1), matching ThreadX initial count. */
    xSemaphoreGive(tm_semaphore_array[semaphore_id]);

    return TM_SUCCESS;
}

int tm_semaphore_get(int semaphore_id)
{
    if (xSemaphoreTake(tm_semaphore_array[semaphore_id], 0) != pdTRUE)
        return TM_ERROR;

    return TM_SUCCESS;
}

int tm_semaphore_put(int semaphore_id)
{
#if defined(__arm__) && !defined(TM_ISR_VIA_THREAD)
    if (xPortIsInsideInterrupt()) {
        BaseType_t yield = pdFALSE;
        if (xSemaphoreGiveFromISR(tm_semaphore_array[semaphore_id], &yield) !=
            pdTRUE)
            return TM_ERROR;
        portYIELD_FROM_ISR(yield);
        return TM_SUCCESS;
    }
#endif
    if (xSemaphoreGive(tm_semaphore_array[semaphore_id]) != pdTRUE)
        return TM_ERROR;

    return TM_SUCCESS;
}


/* Memory pool management -- O(1) freelist */

int tm_memory_pool_create(int pool_id)
{
    int i;
    unsigned char *base = tm_pool_area[pool_id];

    /* Build freelist: each block stores a next pointer. */
    tm_pool_free[pool_id] = (void *) base;
    for (i = 0; i < TM_BLOCK_COUNT - 1; i++) {
        void **block = (void **) (base + (unsigned) i * TM_BLOCK_SIZE);
        *block = (void *) (base + (unsigned) (i + 1) * TM_BLOCK_SIZE);
    }
    /* Last block terminates the list. */
    {
        void **last =
            (void **) (base + (unsigned) (TM_BLOCK_COUNT - 1) * TM_BLOCK_SIZE);
        *last = NULL;
    }

    return TM_SUCCESS;
}

int tm_memory_pool_allocate(int pool_id, unsigned char **memory_ptr)
{
    void *block = tm_pool_free[pool_id];

    if (block == NULL)
        return TM_ERROR;

    /* Pop head of freelist. */
    tm_pool_free[pool_id] = *((void **) block);
    *memory_ptr = (unsigned char *) block;

    return TM_SUCCESS;
}

int tm_memory_pool_deallocate(int pool_id, unsigned char *memory_ptr)
{
    /* Push onto freelist head. */
    *((void **) memory_ptr) = tm_pool_free[pool_id];
    tm_pool_free[pool_id] = (void *) memory_ptr;

    return TM_SUCCESS;
}


/* Low-level character output for tm_printf().
 * Cortex-M semihosting builds use ports/common/cortex-m/tm_putchar.c instead.
 */
#ifndef TM_SEMIHOSTING
void tm_putchar(int c)
{
    putchar(c);
}
#endif
