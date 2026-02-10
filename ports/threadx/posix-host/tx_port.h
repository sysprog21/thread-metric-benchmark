/*
 * POSIX host port for ThreadX -- works on macOS, Linux, and other
 * POSIX-compliant systems.
 *
 * Derived from the ThreadX linux/gnu port with the following changes:
 *   - sem_t replaced by tx_posix_sem_t (pthread mutex + condvar)
 *     because macOS does not support unnamed POSIX semaphores.
 *   - Recursive mutex count tracked manually instead of peeking at
 *     glibc-internal __data.__count.
 *   - CPU affinity (sched_setaffinity) removed; not available on macOS.
 *   - SCHED_FIFO made best-effort (non-fatal if unprivileged).
 *   - Timer uses nanosleep instead of sem_timedwait.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TX_PORT_H
#define TX_PORT_H

/* ------------------------------------------------------------------ */
/*  Optional user overrides                                            */
/* ------------------------------------------------------------------ */

#ifdef TX_INCLUDE_USER_DEFINE_FILE
#include "tx_user.h"
#endif

/* ------------------------------------------------------------------ */
/*  Standard includes                                                  */
/* ------------------------------------------------------------------ */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  Basic ThreadX types                                                */
/* ------------------------------------------------------------------ */

typedef void                    VOID;
typedef char                    CHAR;
typedef unsigned char           UCHAR;
typedef int                     INT;
typedef unsigned int            UINT;
typedef short                   SHORT;
typedef unsigned short          USHORT;
typedef uint64_t                ULONG64;
#define ULONG64_DEFINED

#if defined(__LP64__) || defined(_LP64)
typedef int                     LONG;
typedef unsigned int            ULONG;
#define ALIGN_TYPE_DEFINED
typedef unsigned long long      ALIGN_TYPE;
#define TX_BYTE_BLOCK_FREE      ((ALIGN_TYPE) 0xFFFFEEEEFFFFEEEE)
#else
typedef long                    LONG;
typedef unsigned long           ULONG;
#endif

/* ------------------------------------------------------------------ */
/*  Portable semaphore built on pthread mutex + condvar                */
/*  (macOS does not implement sem_init / sem_timedwait)                */
/* ------------------------------------------------------------------ */

typedef struct
{
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    int             count;
} tx_posix_sem_t;

static inline void tx_posix_sem_init(tx_posix_sem_t *s, int value)
{
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->cond, NULL);
    s->count = value;
}

static inline void tx_posix_sem_post(tx_posix_sem_t *s)
{
    pthread_mutex_lock(&s->lock);
    s->count++;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->lock);
}

static inline void tx_posix_sem_wait(tx_posix_sem_t *s)
{
    pthread_mutex_lock(&s->lock);
    while (s->count <= 0)
        pthread_cond_wait(&s->cond, &s->lock);
    s->count--;
    pthread_mutex_unlock(&s->lock);
}

static inline int tx_posix_sem_trywait(tx_posix_sem_t *s)
{
    int ret;
    pthread_mutex_lock(&s->lock);
    if (s->count > 0) { s->count--; ret = 0; }
    else               { ret = -1; }
    pthread_mutex_unlock(&s->lock);
    return ret;
}

static inline void tx_posix_sem_destroy(tx_posix_sem_t *s)
{
    pthread_mutex_destroy(&s->lock);
    pthread_cond_destroy(&s->cond);
}

/* ------------------------------------------------------------------ */
/*  Configuration knobs                                                */
/* ------------------------------------------------------------------ */

#ifndef TX_MAX_PRIORITIES
#define TX_MAX_PRIORITIES                   32
#endif

#ifndef TX_MINIMUM_STACK
#define TX_MINIMUM_STACK                    200
#endif

#ifndef TX_TIMER_THREAD_STACK_SIZE
#define TX_TIMER_THREAD_STACK_SIZE          400
#endif

#ifndef TX_TIMER_THREAD_PRIORITY
#define TX_TIMER_THREAD_PRIORITY            0
#endif

#ifndef TX_POSIX_MEMORY_SIZE
#define TX_POSIX_MEMORY_SIZE                64000
#endif

/* ------------------------------------------------------------------ */
/*  Interrupt posture constants                                        */
/* ------------------------------------------------------------------ */

#define TX_INT_DISABLE                      1
#define TX_INT_ENABLE                       0

/* ------------------------------------------------------------------ */
/*  TX_MEMSET (avoids C-library dependency in kernel)                  */
/* ------------------------------------------------------------------ */

#ifndef TX_MISRA_ENABLE
#define TX_MEMSET(a,b,c)                    {                           \
                                            UCHAR *ptr;                 \
                                            UCHAR value;                \
                                            UINT  i, size;              \
                                                ptr   = (UCHAR *)((VOID *) a); \
                                                value = (UCHAR) b;      \
                                                size  = (UINT) c;        \
                                                for (i = 0; i < size; i++) \
                                                    *ptr++ = value;      \
                                            }
#endif

/* ------------------------------------------------------------------ */
/*  Trace                                                              */
/* ------------------------------------------------------------------ */

#ifndef TX_TRACE_TIME_SOURCE
#define TX_TRACE_TIME_SOURCE                ((ULONG) (_tx_posix_time_stamp.tv_nsec));
#endif

#ifndef TX_TRACE_TIME_MASK
#define TX_TRACE_TIME_MASK                  0xFFFFFFFFUL
#endif

#define TX_TRACE_PORT_EXTENSION             clock_gettime(CLOCK_REALTIME, &_tx_posix_time_stamp);

/* ------------------------------------------------------------------ */
/*  Build / init options                                               */
/* ------------------------------------------------------------------ */

#define TX_PORT_SPECIFIC_BUILD_OPTIONS      0

#ifdef TX_MISRA_ENABLE
#define TX_DISABLE_INLINE
#else
#define TX_INLINE_INITIALIZATION
#endif

void    _tx_initialize_start_interrupts(void);
#define TX_PORT_SPECIFIC_PRE_SCHEDULER_INITIALIZATION   _tx_initialize_start_interrupts();

#ifndef TX_MISRA_ENABLE
#ifdef TX_ENABLE_STACK_CHECKING
#undef TX_DISABLE_STACK_FILLING
#endif
#endif

/* ------------------------------------------------------------------ */
/*  Thread control-block extensions                                    */
/* ------------------------------------------------------------------ */

#define TX_THREAD_EXTENSION_0               pthread_t       tx_thread_posix_thread_id;      \
                                            tx_posix_sem_t  tx_thread_posix_run_semaphore;   \
                                            UINT            tx_thread_posix_suspension_type; \
                                            UINT            tx_thread_posix_int_disabled_flag;

#define TX_THREAD_EXTENSION_1               VOID       *tx_thread_extension_ptr;
#define TX_THREAD_EXTENSION_2
#define TX_THREAD_EXTENSION_3

/* ------------------------------------------------------------------ */
/*  Object-lifecycle hooks (all empty for this port)                   */
/* ------------------------------------------------------------------ */

#define TX_BLOCK_POOL_EXTENSION
#define TX_BYTE_POOL_EXTENSION
#define TX_EVENT_FLAGS_GROUP_EXTENSION
#define TX_MUTEX_EXTENSION
#define TX_QUEUE_EXTENSION
#define TX_SEMAPHORE_EXTENSION
#define TX_TIMER_EXTENSION

#ifndef TX_THREAD_USER_EXTENSION
#define TX_THREAD_USER_EXTENSION
#endif

#define TX_THREAD_CREATE_EXTENSION(thread_ptr)
#define TX_THREAD_DELETE_EXTENSION(thread_ptr)
#define TX_THREAD_COMPLETED_EXTENSION(thread_ptr)
#define TX_THREAD_TERMINATED_EXTENSION(thread_ptr)

#define TX_BLOCK_POOL_CREATE_EXTENSION(pool_ptr)
#define TX_BYTE_POOL_CREATE_EXTENSION(pool_ptr)
#define TX_EVENT_FLAGS_GROUP_CREATE_EXTENSION(group_ptr)
#define TX_MUTEX_CREATE_EXTENSION(mutex_ptr)
#define TX_QUEUE_CREATE_EXTENSION(queue_ptr)
#define TX_SEMAPHORE_CREATE_EXTENSION(semaphore_ptr)
#define TX_TIMER_CREATE_EXTENSION(timer_ptr)

#define TX_BLOCK_POOL_DELETE_EXTENSION(pool_ptr)
#define TX_BYTE_POOL_DELETE_EXTENSION(pool_ptr)
#define TX_EVENT_FLAGS_GROUP_DELETE_EXTENSION(group_ptr)
#define TX_MUTEX_DELETE_EXTENSION(mutex_ptr)
#define TX_QUEUE_DELETE_EXTENSION(queue_ptr)
#define TX_SEMAPHORE_DELETE_EXTENSION(semaphore_ptr)
#define TX_TIMER_DELETE_EXTENSION(timer_ptr)

/* Thread delete / reset completion (needed by tx_thread_create.c).  */

struct TX_THREAD_STRUCT;

void _tx_thread_delete_port_completion(struct TX_THREAD_STRUCT *thread_ptr, UINT tx_saved_posture);
#define TX_THREAD_DELETE_PORT_COMPLETION(thread_ptr) \
    _tx_thread_delete_port_completion(thread_ptr, tx_saved_posture);

void _tx_thread_reset_port_completion(struct TX_THREAD_STRUCT *thread_ptr, UINT tx_saved_posture);
#define TX_THREAD_RESET_PORT_COMPLETION(thread_ptr) \
    _tx_thread_reset_port_completion(thread_ptr, tx_saved_posture);

/* 64-bit timer extension for thread timeout routing.  */

#if defined(__LP64__) || defined(_LP64)
#define TX_TIMER_INTERNAL_EXTENSION         VOID    *tx_timer_internal_extension_ptr;

#define TX_THREAD_CREATE_TIMEOUT_SETUP(t)                                               \
    (t) -> tx_thread_timer.tx_timer_internal_timeout_function = &(_tx_thread_timeout);   \
    (t) -> tx_thread_timer.tx_timer_internal_timeout_param    = 0;                       \
    (t) -> tx_thread_timer.tx_timer_internal_extension_ptr    = (VOID *) (t);

#define TX_THREAD_TIMEOUT_POINTER_SETUP(t)                                              \
    (t) = (TX_THREAD *) _tx_timer_expired_timer_ptr -> tx_timer_internal_extension_ptr;
#endif

/* ------------------------------------------------------------------ */
/*  Interrupt disable / restore                                        */
/* ------------------------------------------------------------------ */

UINT   _tx_thread_interrupt_disable(void);
VOID   _tx_thread_interrupt_restore(UINT previous_posture);

#define TX_INTERRUPT_SAVE_AREA              UINT    tx_saved_posture;
#define TX_DISABLE                          tx_saved_posture = _tx_thread_interrupt_disable();
#define TX_RESTORE                          _tx_thread_interrupt_restore(tx_saved_posture);

/* ------------------------------------------------------------------ */
/*  POSIX mutex wrappers with manual recursive-lock counter            */
/*  (replaces the glibc __data.__count peek in the Linux port)         */
/* ------------------------------------------------------------------ */

extern pthread_mutex_t  _tx_posix_mutex;
extern __thread int     _tx_posix_mutex_lock_count;

#define tx_posix_mutex_lock(p)   do { pthread_mutex_lock(&(p));   _tx_posix_mutex_lock_count++; } while(0)
#define tx_posix_mutex_unlock(p) do { _tx_posix_mutex_lock_count--; pthread_mutex_unlock(&(p)); } while(0)

#define tx_posix_mutex_recursive_unlock(p) \
    {                                       \
        int _rc = _tx_posix_mutex_lock_count; \
        while (_rc > 0)                     \
        {                                   \
            _tx_posix_mutex_lock_count--;   \
            pthread_mutex_unlock(&(p));     \
            _rc--;                          \
        }                                   \
    }

/* Semaphore post while holding the scheduler mutex (matches the Linux
   port's tx_linux_sem_post pattern).  */
#define tx_posix_sem_post_sched(p)          tx_posix_mutex_lock(_tx_posix_mutex);    \
                                            tx_posix_sem_post(p);                    \
                                            tx_posix_mutex_unlock(_tx_posix_mutex)

/* ------------------------------------------------------------------ */
/*  Per-object interrupt lockout (all map to TX_DISABLE)               */
/* ------------------------------------------------------------------ */

#define TX_BLOCK_POOL_DISABLE               TX_DISABLE
#define TX_BYTE_POOL_DISABLE                TX_DISABLE
#define TX_EVENT_FLAGS_GROUP_DISABLE        TX_DISABLE
#define TX_MUTEX_DISABLE                    TX_DISABLE
#define TX_QUEUE_DISABLE                    TX_DISABLE
#define TX_SEMAPHORE_DISABLE                TX_DISABLE

/* ------------------------------------------------------------------ */
/*  Version string                                                     */
/* ------------------------------------------------------------------ */

#ifdef TX_THREAD_INIT
CHAR                            _tx_version_id[] =
                                    "Copyright (c) Microsoft Corporation * ThreadX POSIX/gcc *";
#else
extern  CHAR                    _tx_version_id[];
#endif

/* ------------------------------------------------------------------ */
/*  Port externals                                                     */
/* ------------------------------------------------------------------ */

extern tx_posix_sem_t                   _tx_posix_semaphore;
extern tx_posix_sem_t                   _tx_posix_semaphore_no_idle;
extern ULONG                            _tx_posix_global_int_disabled_flag;
extern struct timespec                  _tx_posix_time_stamp;
extern __thread int                     _tx_posix_threadx_thread;

void    _tx_posix_thread_suspend(pthread_t thread_id);
void    _tx_posix_thread_resume(pthread_t thread_id);
void    _tx_posix_thread_init(void);

#define TX_POSIX_PRIORITY_SCHEDULE          (3)
#define TX_POSIX_PRIORITY_ISR               (2)
#define TX_POSIX_PRIORITY_USER_THREAD       (1)

#endif /* TX_PORT_H */
