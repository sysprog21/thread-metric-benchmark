/*
 * POSIX host port -- system return (thread yields to scheduler).
 * SPDX-License-Identifier: MIT
 */

#define TX_SOURCE_CODE

#include "tx_api.h"
#include "tx_thread.h"
#include "tx_timer.h"

VOID   _tx_thread_system_return(VOID)
{
    TX_THREAD      *temp_thread_ptr;
    tx_posix_sem_t *temp_run_semaphore;
    UINT            temp_thread_state;
    pthread_t       thread_id;
    int             exit_code = 0;

    tx_posix_mutex_lock(_tx_posix_mutex);

    thread_id       = pthread_self();
    temp_thread_ptr = _tx_thread_current_ptr;

    /* Detect terminated thread still running for cleanup.  */
    if ((_tx_posix_threadx_thread) &&
        ((!temp_thread_ptr) ||
         (!pthread_equal(temp_thread_ptr -> tx_thread_posix_thread_id,
                         thread_id))))
    {
        tx_posix_mutex_recursive_unlock(_tx_posix_mutex);
        pthread_exit((void *) &exit_code);
    }

    /* Preserve time-slice.  */
    if (_tx_timer_time_slice)
    {
        temp_thread_ptr -> tx_thread_time_slice = _tx_timer_time_slice;
        _tx_timer_time_slice = 0;
    }

    temp_run_semaphore = &temp_thread_ptr -> tx_thread_posix_run_semaphore;
    temp_thread_state  = temp_thread_ptr -> tx_thread_state;

    temp_thread_ptr -> tx_thread_posix_suspension_type = 0;
    _tx_thread_current_ptr = TX_NULL;

    tx_posix_mutex_recursive_unlock(_tx_posix_mutex);

    /* Drain and post the scheduler semaphore.  */
    while (!tx_posix_sem_trywait(&_tx_posix_semaphore))
        ;
    tx_posix_sem_post_sched(&_tx_posix_semaphore);

    /* If the thread self-terminated, exit the pthread.  */
    if (temp_thread_state == TX_TERMINATED)
        pthread_exit((void *) &exit_code);

    /* Wait until the scheduler re-selects this thread.  */
    tx_posix_sem_wait(temp_run_semaphore);
    tx_posix_sem_post(&_tx_posix_semaphore);

    tx_posix_mutex_lock(_tx_posix_mutex);

    /* Re-check for termination after wakeup.  */
    temp_thread_ptr = _tx_thread_current_ptr;
    if ((_tx_posix_threadx_thread) &&
        ((!temp_thread_ptr) ||
         (!pthread_equal(temp_thread_ptr -> tx_thread_posix_thread_id,
                         thread_id))))
    {
        tx_posix_mutex_recursive_unlock(_tx_posix_mutex);
        pthread_exit((void *) &exit_code);
    }

    if (!_tx_thread_current_ptr -> tx_thread_posix_int_disabled_flag)
        tx_posix_mutex_recursive_unlock(_tx_posix_mutex);
}
