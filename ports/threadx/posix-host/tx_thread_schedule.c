/*
 * POSIX host port -- thread scheduler.
 * SPDX-License-Identifier: MIT
 */

#define TX_SOURCE_CODE

#include "tx_api.h"
#include "tx_thread.h"
#include "tx_timer.h"
#include <errno.h>

extern tx_posix_sem_t   _tx_posix_isr_semaphore;
extern UINT             _tx_posix_timer_waiting;
extern pthread_t        _tx_posix_timer_id;

VOID   _tx_thread_schedule(VOID)
{
    struct timespec ts;

    ts.tv_sec  = 0;
    ts.tv_nsec = 200000;

    while (1)
    {
        /* Wait for a runnable thread while no ISR is active.  */
        while (1)
        {
            tx_posix_mutex_lock(_tx_posix_mutex);

            if ((_tx_thread_execute_ptr != TX_NULL) &&
                (_tx_thread_system_state == 0))
            {
                break;
            }

            tx_posix_mutex_unlock(_tx_posix_mutex);
            nanosleep(&ts, NULL);
        }

        /* Schedule the next thread (mutex is held).  */
        _tx_thread_current_ptr = _tx_thread_execute_ptr;
        _tx_thread_current_ptr -> tx_thread_run_count++;
        _tx_timer_time_slice = _tx_thread_current_ptr -> tx_thread_time_slice;

        if (_tx_thread_current_ptr -> tx_thread_posix_suspension_type)
        {
            /* Pseudo-interrupt suspension -- resume the pthread.  */
            _tx_posix_thread_resume(
                _tx_thread_current_ptr -> tx_thread_posix_thread_id);
        }
        else
        {
            /* Drain and post the run semaphore.  */
            while (!tx_posix_sem_trywait(
                &_tx_thread_current_ptr -> tx_thread_posix_run_semaphore))
                ;
            tx_posix_sem_post_sched(
                &_tx_thread_current_ptr -> tx_thread_posix_run_semaphore);

            if (_tx_posix_timer_waiting)
            {
                tx_posix_sem_wait(&_tx_posix_semaphore);
                tx_posix_sem_post(&_tx_posix_isr_semaphore);
            }
            else
            {
                _tx_posix_thread_suspend(_tx_posix_timer_id);
                tx_posix_sem_wait(&_tx_posix_semaphore);
                _tx_posix_thread_resume(_tx_posix_timer_id);
            }
        }

        tx_posix_mutex_unlock(_tx_posix_mutex);

        /* Block until the thread yields back.  */
        tx_posix_sem_wait(&_tx_posix_semaphore);
    }
}

/* ------------------------------------------------------------------ */
/*  Port-completion helpers for thread delete / reset                   */
/* ------------------------------------------------------------------ */

void _tx_thread_delete_port_completion(TX_THREAD *thread_ptr,
                                       UINT tx_saved_posture)
{
    pthread_t       tid;
    tx_posix_sem_t *sem;
    struct timespec ts;
    int             rc;

    tid = thread_ptr -> tx_thread_posix_thread_id;
    sem = &thread_ptr -> tx_thread_posix_run_semaphore;
    ts.tv_sec  = 0;
    ts.tv_nsec = 1000000;

    TX_RESTORE
    do
    {
        rc = pthread_cancel(tid);
        if (rc != EAGAIN)
            break;
        _tx_posix_thread_resume(tid);
        tx_posix_sem_post(sem);
        nanosleep(&ts, NULL);
    } while (1);
    pthread_join(tid, NULL);
    tx_posix_sem_destroy(sem);
    TX_DISABLE
}

void _tx_thread_reset_port_completion(TX_THREAD *thread_ptr,
                                      UINT tx_saved_posture)
{
    pthread_t       tid;
    tx_posix_sem_t *sem;
    struct timespec ts;
    int             rc;

    tid = thread_ptr -> tx_thread_posix_thread_id;
    sem = &thread_ptr -> tx_thread_posix_run_semaphore;
    ts.tv_sec  = 0;
    ts.tv_nsec = 1000000;

    TX_RESTORE
    do
    {
        rc = pthread_cancel(tid);
        if (rc != EAGAIN)
            break;
        _tx_posix_thread_resume(tid);
        tx_posix_sem_post(sem);
        nanosleep(&ts, NULL);
    } while (1);
    pthread_join(tid, NULL);
    tx_posix_sem_destroy(sem);
    TX_DISABLE
}
