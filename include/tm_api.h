/*
 * Copyright (c) 2024 Microsoft Corporation
 * SPDX-License-Identifier: MIT
 */

/* Thread-Metric Component -- Application Interface (API)
 *
 * RTOS-neutral API for the Thread-Metric performance test suite.
 * All service prototypes and data structure definitions are here.
 */

#ifndef TM_API_H
#define TM_API_H

/* Determine if a C++ compiler is being used.  If so, ensure that standard
 * C is used to process the API information.
 */

#ifdef __cplusplus

/* Yes, C++ compiler is present.  Use standard C. */
extern "C" {

#endif

/* Define API constants. */

#define TM_SUCCESS 0
#define TM_ERROR 1


/* Define the time interval in seconds. This can be changed with a -D compiler
 * option.
 */

#ifndef TM_TEST_DURATION
#define TM_TEST_DURATION 30
#endif

/* Number of reporting cycles before the test calls exit(0).  Default 0
 * means infinite (original behavior).  Set to 1 for CI / QEMU semihosting
 * runs so the program terminates cleanly via _exit() -> SYS_EXIT.
 */
#ifndef TM_TEST_CYCLES
#define TM_TEST_CYCLES 0
#endif

/* Report helpers and tiny printf implemented in src/tm_report.c.
 * tm_putchar() is the only function each porting layer must supply
 * for console output; tm_printf() calls it internally.
 */
void tm_report_finish(void);
void tm_check_fail(const char *msg);
void tm_putchar(int c);
void tm_printf(const char *fmt, ...);

/* Reporter loop helpers -- centralise the bounded-cycle logic so every
 * test file does not duplicate it.  C89 compatible.  Usage:
 *     TM_REPORT_LOOP {
 *         ... sleep, print, check counters ...
 *     } TM_REPORT_FINISH
 */
#define TM_REPORT_LOOP                                          \
    {                                                           \
        int _tm_cycle;                                          \
        for (_tm_cycle = 0;                                     \
             !TM_TEST_CYCLES || _tm_cycle < TM_TEST_CYCLES;     \
             _tm_cycle++)

#define TM_REPORT_FINISH                                        \
    }                                                           \
    tm_report_finish()

/* Init-time check: abort on failure so mis-configured porting layers
 * are caught immediately instead of producing silent hangs.
 */
#define TM_CHECK(call)                                          \
    do {                                                        \
        if ((call) != TM_SUCCESS)                               \
            tm_check_fail("FATAL: " #call " failed\n");         \
    } while (0)


/* Define RTOS Neutral APIs. RTOS vendors should fill in the guts of the
 * following API. Once this is done the Thread-Metric tests can be successfully
 * run.
 */

void tm_initialize(void (*test_initialization_function)(void));
int tm_thread_create(int thread_id, int priority, void (*entry_function)(void));
int tm_thread_resume(int thread_id);
int tm_thread_suspend(int thread_id);
void tm_thread_relinquish(void);
void tm_thread_sleep(int seconds);
int tm_queue_create(int queue_id);
int tm_queue_send(int queue_id, unsigned long *message_ptr);
int tm_queue_receive(int queue_id, unsigned long *message_ptr);
int tm_semaphore_create(int semaphore_id);
int tm_semaphore_get(int semaphore_id);
int tm_semaphore_put(int semaphore_id);
int tm_memory_pool_create(int pool_id);
int tm_memory_pool_allocate(int pool_id, unsigned char **memory_ptr);
int tm_memory_pool_deallocate(int pool_id, unsigned char *memory_ptr);
void tm_cause_interrupt(void);


/* Determine if a C++ compiler is being used.  If so, complete the standard
 * C conditional started above.
 */
#ifdef __cplusplus
}
#endif

#endif
