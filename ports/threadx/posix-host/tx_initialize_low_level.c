/*
 * POSIX host port -- low-level initialization.
 *
 * Differences from the Linux port:
 *   - No CPU affinity (sched_setaffinity is Linux-only).
 *   - Timer uses nanosleep instead of sem_timedwait.
 *   - SCHED_FIFO is best-effort (non-fatal when unprivileged).
 *
 * SPDX-License-Identifier: MIT
 */

#define TX_SOURCE_CODE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "tx_api.h"

/* ------------------------------------------------------------------ */
/*  Global objects                                                     */
/* ------------------------------------------------------------------ */

pthread_mutex_t _tx_posix_mutex;
__thread int _tx_posix_mutex_lock_count = 0;
tx_posix_sem_t _tx_posix_semaphore;
tx_posix_sem_t _tx_posix_semaphore_no_idle;
ULONG _tx_posix_global_int_disabled_flag;
struct timespec _tx_posix_time_stamp;
__thread int _tx_posix_threadx_thread = 0;

/* Signals used to suspend / resume pthreads.  */
#define SUSPEND_SIG SIGUSR1
#define RESUME_SIG SIGUSR2

static sigset_t _tx_posix_thread_wait_mask;
static __thread int _tx_posix_thread_suspended;
static int _tx_posix_thread_timer_pipe[2];
static int _tx_posix_thread_other_pipe[2];

/* Timer thread.  */
pthread_t _tx_posix_timer_id;
tx_posix_sem_t _tx_posix_timer_semaphore;
tx_posix_sem_t _tx_posix_isr_semaphore;
static void *_tx_posix_timer_interrupt(void *p);

/* Signal handlers.  */
static void _tx_posix_thread_resume_handler(int sig)
{
    (void) sig;
}

static void _tx_posix_thread_suspend_handler(int sig)
{
    unsigned char byte = 1;
    int fd;

    (void) sig;

    /* Already suspended (duplicate signal) -- ignore without pushing
       an extra ack byte that would desynchronize the read/write pairing.  */
    if (_tx_posix_thread_suspended)
        return;

    /* Pick the ack pipe for this thread's role.  */
    fd = pthread_equal(pthread_self(), _tx_posix_timer_id)
             ? _tx_posix_thread_timer_pipe[1]
             : _tx_posix_thread_other_pipe[1];

    /* write() is async-signal-safe.  The write end is O_NONBLOCK, so
       this cannot block even if the pipe buffer is full (which would
       indicate a bug in the caller, not something we can fix here).  */
    (void) write(fd, &byte, 1);

    _tx_posix_thread_suspended = 1;
    sigsuspend(&_tx_posix_thread_wait_mask);
    _tx_posix_thread_suspended = 0;
}

/* Forward declarations expected by ThreadX core.  */
extern void _tx_timer_interrupt(void);
extern VOID _tx_thread_context_save(VOID);
extern VOID _tx_thread_context_restore(VOID);
extern VOID *_tx_initialize_unused_memory;

/* ------------------------------------------------------------------ */
/*  _tx_initialize_low_level                                           */
/* ------------------------------------------------------------------ */

VOID _tx_initialize_low_level(VOID)
{
    struct sched_param sp;
    pthread_mutexattr_t attr;

    _tx_initialize_unused_memory = malloc(TX_POSIX_MEMORY_SIZE);
    if (!_tx_initialize_unused_memory) {
        printf("ThreadX POSIX error allocating memory!\n");
        while (1)
            ;
    }

    _tx_posix_thread_init();

    /* Try to elevate the scheduler thread.  Non-fatal if we lack
       permission -- the benchmark still works, just less deterministic.
       Skip on macOS where pthread_setschedparam(SCHED_FIFO) blocks.  */
#ifdef __linux__
    sp.sched_priority = TX_POSIX_PRIORITY_SCHEDULE;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
#else
    (void) sp;
#endif

    /* Recursive mutex for the global critical section.  */
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_tx_posix_mutex, &attr);

    tx_posix_sem_init(&_tx_posix_semaphore, 0);
#ifdef TX_LINUX_NO_IDLE_ENABLE
    tx_posix_sem_init(&_tx_posix_semaphore_no_idle, 0);
#endif

    _tx_posix_global_int_disabled_flag = TX_FALSE;

    tx_posix_sem_init(&_tx_posix_timer_semaphore, 0);
    tx_posix_sem_init(&_tx_posix_isr_semaphore, 0);

    if (pthread_create(&_tx_posix_timer_id, NULL, _tx_posix_timer_interrupt,
                       NULL)) {
        printf("ThreadX POSIX error creating timer thread!\n");
        while (1)
            ;
    }

#ifdef __linux__
    sp.sched_priority = TX_POSIX_PRIORITY_ISR;
    pthread_setschedparam(_tx_posix_timer_id, SCHED_FIFO, &sp);
#endif
}

/* ------------------------------------------------------------------ */
/*  _tx_initialize_start_interrupts                                    */
/* ------------------------------------------------------------------ */

void _tx_initialize_start_interrupts(void)
{
    tx_posix_sem_post_sched(&_tx_posix_timer_semaphore);
}

/* ------------------------------------------------------------------ */
/*  Timer interrupt thread (uses nanosleep -- portable)                */
/* ------------------------------------------------------------------ */

static void *_tx_posix_timer_interrupt(void *p)
{
    struct timespec ts;
    long nsec;

    (void) p;
    nsec = 1000000000L / TX_TIMER_TICKS_PER_SECOND;

    /* Wait for the kernel to start.  */
    tx_posix_sem_wait(&_tx_posix_timer_semaphore);

    while (1) {
        ts.tv_sec = 0;
        ts.tv_nsec = nsec;
        nanosleep(&ts, NULL);

        _tx_thread_context_save();
        _tx_trace_isr_enter_insert(0);
        _tx_timer_interrupt();
        _tx_trace_isr_exit_insert(0);
        _tx_thread_context_restore();

#ifdef TX_LINUX_NO_IDLE_ENABLE
        tx_posix_mutex_lock(_tx_posix_mutex);
        while (!tx_posix_sem_trywait(&_tx_posix_semaphore_no_idle))
            ;
        tx_posix_sem_post(&_tx_posix_semaphore_no_idle);
        tx_posix_mutex_unlock(_tx_posix_mutex);
#endif
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Thread suspend / resume (POSIX signals -- works on macOS & Linux)  */
/* ------------------------------------------------------------------ */

void _tx_posix_thread_suspend(pthread_t thread_id)
{
    unsigned char byte;
    int fd;

    tx_posix_mutex_lock(_tx_posix_mutex);
    pthread_kill(thread_id, SUSPEND_SIG);
    tx_posix_mutex_unlock(_tx_posix_mutex);

    /* Block until the signal handler writes an ack byte.
       Retry on EINTR -- signals can interrupt read().  */
    fd = pthread_equal(thread_id, _tx_posix_timer_id)
             ? _tx_posix_thread_timer_pipe[0]
             : _tx_posix_thread_other_pipe[0];

    while (read(fd, &byte, 1) < 0 && errno == EINTR)
        ;
}

void _tx_posix_thread_resume(pthread_t thread_id)
{
    tx_posix_mutex_lock(_tx_posix_mutex);
    pthread_kill(thread_id, RESUME_SIG);
    tx_posix_mutex_unlock(_tx_posix_mutex);
}

void _tx_posix_thread_init(void)
{
    struct sigaction sa;
    sigset_t block_set;

    if (pipe(_tx_posix_thread_timer_pipe) || pipe(_tx_posix_thread_other_pipe)) {
        printf("ThreadX POSIX error creating pipes!\n");
        while (1)
            ;
    }

    /* Make the write ends non-blocking so the signal handler can never
       stall.  The read ends stay blocking (callers retry on EINTR).  */
    fcntl(_tx_posix_thread_timer_pipe[1], F_SETFL, O_NONBLOCK);
    fcntl(_tx_posix_thread_other_pipe[1], F_SETFL, O_NONBLOCK);

    sigfillset(&_tx_posix_thread_wait_mask);
    sigdelset(&_tx_posix_thread_wait_mask, RESUME_SIG);

    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = _tx_posix_thread_resume_handler;
    sigaction(RESUME_SIG, &sa, NULL);

    sa.sa_handler = _tx_posix_thread_suspend_handler;
    sigaction(SUSPEND_SIG, &sa, NULL);

    /* Block RESUME_SIG in the calling thread's mask.  All subsequently
       created pthreads inherit this mask.  sigsuspend() in the suspend
       handler atomically unblocks it when waiting for the resume signal.  */
    sigemptyset(&block_set);
    sigaddset(&block_set, RESUME_SIG);
    pthread_sigmask(SIG_BLOCK, &block_set, NULL);
}
