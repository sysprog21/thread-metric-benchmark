/*
 * POSIX host port -- timer interrupt processing.
 * SPDX-License-Identifier: MIT
 */

#define TX_SOURCE_CODE

#include "tx_api.h"
#include "tx_timer.h"
#include "tx_thread.h"

VOID   _tx_timer_interrupt(VOID)
{
    tx_posix_mutex_lock(_tx_posix_mutex);

    _tx_timer_system_clock++;

    if (_tx_timer_time_slice)
    {
        _tx_timer_time_slice--;
        if (_tx_timer_time_slice == 0)
            _tx_timer_expired_time_slice = TX_TRUE;
    }

    if (*_tx_timer_current_ptr)
    {
        _tx_timer_expired = TX_TRUE;
    }
    else
    {
        _tx_timer_current_ptr++;
        if (_tx_timer_current_ptr == _tx_timer_list_end)
            _tx_timer_current_ptr = _tx_timer_list_start;
    }

    if ((_tx_timer_expired_time_slice) || (_tx_timer_expired))
    {
        if (_tx_timer_expired)
            _tx_timer_expiration_process();

        if (_tx_timer_expired_time_slice)
            _tx_thread_time_slice();
    }

    tx_posix_mutex_unlock(_tx_posix_mutex);
}
