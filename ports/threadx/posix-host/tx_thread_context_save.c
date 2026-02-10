/*
 * POSIX host port -- context save (ISR entry).
 * SPDX-License-Identifier: MIT
 */

#define TX_SOURCE_CODE

#include "tx_api.h"
#include "tx_thread.h"
#include "tx_timer.h"

VOID   _tx_thread_context_save(VOID)
{
    tx_posix_mutex_lock(_tx_posix_mutex);

    if ((_tx_thread_current_ptr) && (_tx_thread_system_state == 0))
    {
        _tx_posix_thread_suspend(
            _tx_thread_current_ptr -> tx_thread_posix_thread_id);
        _tx_thread_current_ptr -> tx_thread_posix_suspension_type = 1;
    }

    _tx_thread_system_state++;

    tx_posix_mutex_unlock(_tx_posix_mutex);
}
