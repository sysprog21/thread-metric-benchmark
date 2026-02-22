/*
 * POSIX host port -- thread stack build (creates pthreads).
 * SPDX-License-Identifier: MIT
 */

#define TX_SOURCE_CODE

#include <stdio.h>
#include "tx_api.h"
#include "tx_thread.h"

static void *_tx_posix_thread_entry(void *ptr);

VOID _tx_thread_stack_build(TX_THREAD *thread_ptr, VOID (*function_ptr)(VOID))
{
    struct sched_param sp;

    (void) function_ptr;

    tx_posix_sem_init(&thread_ptr->tx_thread_posix_run_semaphore, 0);

    if (pthread_create(&thread_ptr->tx_thread_posix_thread_id, NULL,
                       _tx_posix_thread_entry, thread_ptr)) {
        printf("ThreadX POSIX error creating thread!\n");
        while (1) {
        }
    }

    /* Best-effort priority elevation (skip on macOS where it blocks). */
#ifdef __linux__
    sp.sched_priority = TX_POSIX_PRIORITY_USER_THREAD;
    pthread_setschedparam(thread_ptr->tx_thread_posix_thread_id, SCHED_FIFO,
                          &sp);
#else
    (void) sp;
#endif

    thread_ptr->tx_thread_posix_suspension_type = 0;
    thread_ptr->tx_thread_posix_int_disabled_flag = 0;
    thread_ptr->tx_thread_stack_ptr =
        (VOID *) (((CHAR *) thread_ptr->tx_thread_stack_end) - 8);
    *(((ULONG *) thread_ptr->tx_thread_stack_ptr) - 1) = 0;
}

static void *_tx_posix_thread_entry(void *ptr)
{
    TX_THREAD *thread_ptr = (TX_THREAD *) ptr;

    _tx_posix_threadx_thread = 1;

    /* Wait until the scheduler lets us run. */
    tx_posix_sem_wait(&thread_ptr->tx_thread_posix_run_semaphore);
    tx_posix_sem_post(&_tx_posix_semaphore);

    _tx_thread_shell_entry();

    return NULL;
}
