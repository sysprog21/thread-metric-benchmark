/*
 * POSIX host port -- interrupt disable / enable via recursive mutex.
 * SPDX-License-Identifier: MIT
 */

#define TX_SOURCE_CODE

#include "tx_api.h"
#include "tx_thread.h"

UINT _tx_thread_interrupt_disable(void)
{
    UINT previous = _tx_thread_interrupt_control(TX_INT_DISABLE);
    return previous;
}

VOID _tx_thread_interrupt_restore(UINT previous_posture)
{
    _tx_thread_interrupt_control(previous_posture);
}

UINT _tx_thread_interrupt_control(UINT new_posture)
{
    UINT old_posture;
    TX_THREAD *thread_ptr;
    pthread_t thread_id;
    int exit_code = 0;

    tx_posix_mutex_lock(_tx_posix_mutex);

    thread_id = pthread_self();
    thread_ptr = _tx_thread_current_ptr;

    /* Detect terminated thread still running for cleanup.  */
    if ((_tx_posix_threadx_thread) &&
        ((!thread_ptr) ||
         (!pthread_equal(thread_ptr->tx_thread_posix_thread_id, thread_id)))) {
        tx_posix_mutex_recursive_unlock(_tx_posix_mutex);
        pthread_exit((void *) &exit_code);
    }

    /* Determine current posture from recursive lock depth.  */
    old_posture =
        (_tx_posix_mutex_lock_count == 1) ? TX_INT_ENABLE : TX_INT_DISABLE;

    if (_tx_thread_system_state) {
        if (new_posture == TX_INT_ENABLE) {
            _tx_posix_global_int_disabled_flag = TX_FALSE;
            tx_posix_mutex_recursive_unlock(_tx_posix_mutex);
        } else if (new_posture == TX_INT_DISABLE) {
            _tx_posix_global_int_disabled_flag = TX_TRUE;
        }
    } else if (thread_ptr) {
        if (new_posture == TX_INT_ENABLE) {
            _tx_thread_current_ptr->tx_thread_posix_int_disabled_flag =
                TX_FALSE;
            tx_posix_mutex_recursive_unlock(_tx_posix_mutex);
        } else if (new_posture == TX_INT_DISABLE) {
            _tx_thread_current_ptr->tx_thread_posix_int_disabled_flag = TX_TRUE;
        }
    }

    return old_posture;
}
