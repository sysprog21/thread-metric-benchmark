/*
 * POSIX host port -- context restore (ISR exit).
 * SPDX-License-Identifier: MIT
 */

#define TX_SOURCE_CODE

#include "tx_api.h"
#include "tx_thread.h"
#include "tx_timer.h"

extern tx_posix_sem_t   _tx_posix_isr_semaphore;
UINT _tx_posix_timer_waiting = 0;

VOID   _tx_thread_context_restore(VOID)
{
    tx_posix_mutex_lock(_tx_posix_mutex);

    _tx_thread_system_state--;

    if ((!_tx_thread_system_state) && (_tx_thread_current_ptr))
    {
        if ((_tx_thread_preempt_disable == 0) &&
            (_tx_thread_current_ptr != _tx_thread_execute_ptr))
        {
            _tx_thread_current_ptr -> tx_thread_posix_suspension_type = 1;

            if (_tx_timer_time_slice)
            {
                _tx_thread_current_ptr -> tx_thread_time_slice =
                    _tx_timer_time_slice;
                _tx_timer_time_slice = 0;
            }

            _tx_thread_current_ptr = TX_NULL;

            while (!tx_posix_sem_trywait(&_tx_posix_semaphore))
                ;

            _tx_posix_timer_waiting = 1;

            tx_posix_sem_post_sched(&_tx_posix_semaphore);

            if (_tx_thread_execute_ptr)
            {
                if (_tx_thread_execute_ptr -> tx_thread_posix_suspension_type == 0)
                {
                    tx_posix_mutex_recursive_unlock(_tx_posix_mutex);
                    tx_posix_sem_wait(&_tx_posix_isr_semaphore);
                    tx_posix_mutex_lock(_tx_posix_mutex);
                    while (!tx_posix_sem_trywait(&_tx_posix_isr_semaphore))
                        ;
                }
            }

            _tx_posix_timer_waiting = 0;
        }
        else
        {
            _tx_posix_thread_resume(
                _tx_thread_current_ptr -> tx_thread_posix_thread_id);
        }
    }

    tx_posix_mutex_recursive_unlock(_tx_posix_mutex);
}
