// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

namespace __dios
{

static_assert( _HOST_pthr_mutex_t_size >= 16 );

// this is not run when thread's main returns!
static void _run_cleanup_handlers() noexcept
{
    _PThread &thread = getThread();

    CleanupHandler *handler = thread.cleanup_handlers;
    thread.cleanup_handlers = NULL;
    CleanupHandler *next;

    while ( handler ) {
        next = handler->next;
        handler->routine( handler->arg );
        __vm_obj_free( handler );
        handler = next;
    }
}

void _cancel( __dios::FencedInterruptMask &mask ) noexcept
{
    __dios_task tid = __dios_this_task();
    _PThread &thread = getThread( tid );
    thread.sleeping = NotSleeping;

    // call all cleanup handlers
    _run_cleanup_handlers();
    _clean_and_become_zombie( mask, tid );
}

/* Function __pthread_entry associates current task with a new thread and
 * executes the thread subroutine. In the end it releases thread resources.
 *
 * After the execution of the thread DiOS terminates current task.
 * */
extern "C" void __pthread_entry( void *_args )
{
    __pthread_start( _args );
    __dios_suicide();
}

static int _pthread_join( __dios::FencedInterruptMask &mask, pthread_t gtid, void **result ) noexcept {
    _PThread &thread = getThread( gtid );

    if ( gtid == __dios_this_task() )
        return EDEADLK;

    if ( thread.detached )
        return EINVAL;

    // wait for the thread to finnish
    waitOrCancel( mask, [&] { return thread.running; } );

    if ( thread.detached )
        // meanwhile detached
        return EINVAL;

    // copy result
    if ( result ) {
        if ( thread.cancelled )
            *result = PTHREAD_CANCELED;
        else
            *result = thread.result;
    }

    // kill the thread so that it does not pollute state space by ending
    // nondeterministically
    releaseAndKillThread( gtid );
    return 0;
}

}

using namespace __dios;


/* The pthread_create() function starts a new thread in the calling
 * process. The new thread starts execution by invoking entry();
 * arg is passed as the sole argument of entry().
 *
 * Function creates a new __dios_task which calls __pthread_entry with
 * thread entry function and its argument arg.
 *
 * The newly created thread is in running state.
 * */
int pthread_create( pthread_t *ptid, const pthread_attr_t *attr, void *( *entry )( void * ), void *arg ) noexcept {
    __dios::FencedInterruptMask mask;

    // test input arguments
    __dios_assert( entry );
    if ( ptid == NULL || entry == NULL )
        return EINVAL;

    // create new thread and pass arguments to the entry wrapper
    Entry *args = static_cast< Entry * >( __vm_obj_make( sizeof( Entry ), _VM_PT_Heap ) );
    args->entry = entry;
    args->arg = arg;
    auto tid = __dios_start_task( __pthread_entry, static_cast< void * >( args ), 0 );
    // init thread here, before it has first chance to run
    __init_thread( tid, attr == nullptr ? PTHREAD_CREATE_JOINABLE : *attr );
    getThread( tid ).entry = args;
    *ptid = tid;

    return 0;
}

/* DESCRIPTION
 * The  pthread_exit() function terminates the calling thread and returns
 * a value via result that (if the thread is joinable)  is  available  to
 * another thread in the same process that calls pthread_join(3).
 *
 * Any clean-up handlers established by pthread_cleanup_push(3) that have
 * not yet been popped, are popped (in the reverse of the order in  which
 * they were pushed) and executed.  If the thread has any thread-specific
 * data, then, after the clean-up handlers have been executed, the corre-
 * sponding destructor functions are called, in an unspecified order.
 *
 * When  a  thread  terminates,  process-shared resources (e.g., mutexes,
 * condition variables, semaphores, and file  descriptors)  are  not  re‐
 * leased, and functions registered using atexit(3) are not called.
 *
 * After  the last thread in a process terminates, the process terminates
 * as by calling exit(3) with an exit  status  of  zero;  thus,  process-
 * shared resources are released and functions registered using atexit(3)
 * are called.
 *
 * RETURN VALUE
 * This function does not return to the caller.
 *
 * ERRORS
 * This function always succeeds.
 */
void pthread_exit( void *result ) noexcept {
    __dios::FencedInterruptMask mask;

    auto gtid = __dios_this_task();
    _PThread &thread = getThread( gtid );
    thread.result = result;

    // in main wait for all the threads
    if ( thread.is_main )
        iterateThreads( [&]( __dios_task tid ) {
                _pthread_join( mask, tid, nullptr ); // joining self is detected and ignored
            } );

    _run_cleanup_handlers();
    _clean_and_become_zombie( mask, gtid );
}

/* DESCRIPTION
 * The  pthread_join()  function waits for the thread specified by thread
 * to  terminate.   If  that  thread   has   already   terminated,   then
 * pthread_join()  returns  immediately.   The thread specified by thread
 * must be joinable.
 *
 * If retval is not NULL, then pthread_join() copies the exit  status  of
 * the  target thread (i.e., the value that the target thread supplied to
 * pthread_exit(3)) into the location pointed to by retval.  If the  tar‐
 * get  thread was canceled, then PTHREAD_CANCELED is placed in the loca‐
 * tion pointed to by retval.
 *
 * If multiple threads simultaneously try to join with the  same  thread,
 * the  results  are  undefined.  If the thread calling pthread_join() is
 * canceled, then the target thread will remain joinable (i.e.,  it  will
 * not be detached).
 *
 * RETURN VALUE
 * On success, pthread_join() returns 0; on error, it returns an error
 * number.
 *
 * ERRORS
 * EDEADLK
 *   A deadlock was detected (e.g., two threads tried to  join  with
 *   each other); or thread specifies the calling thread.
 * EINVAL thread is not a joinable thread.
 *
 * EINVAL Another thread is already waiting to join with this thread.
 *
 * TODO not detecting:
 * ESRCH  No thread with the ID thread could be found.
 */
int pthread_join( pthread_t gtid, void **result ) noexcept {
    __dios::FencedInterruptMask mask;
    return _pthread_join( mask, gtid, result );
}

/* DESCRIPTION
 * The pthread_detach() function marks the thread identified by thread as
 * detached.  When a detached thread terminates, its resources are  auto‐
 * matically  released  back  to  the system without the need for another
 * thread to join with the terminated thread.
 *
 * Attempting to detach an already detached thread results in unspecified
 * behavior.
 *
 * RETURN VALUE
 * On success, pthread_detach() returns 0; on error, it returns an error
 * number.
 *
 * ERRORS
 * EINVAL thread is not a joinable thread.
 *
 * TODO not detecting:
 * ESRCH  No thread with the ID thread could be found.
 */
int pthread_detach( pthread_t gtid ) noexcept {
    __dios::FencedInterruptMask mask;
    _PThread &thread = getThread( gtid );

    if ( thread.detached )
        return EINVAL;

    bool ended = !thread.running;
    thread.detached = true;

    if ( ended ) {
        // kill the thread so that it does not pollute state space by ending
        // nondeterministically
        releaseAndKillThread( gtid );
    }
    return 0;
}

/* Thread attributes */

/*
   pthread_attr_t representation:
     ------------------------------
    | *free* | detach state: 1 bit |
     ------------------------------
  */

int pthread_attr_destroy( pthread_attr_t * ) noexcept {
    __dios::FencedInterruptMask mask;
    return 0;
}

int pthread_attr_init( pthread_attr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;
    *attr = 0;
    return 0;
}

int pthread_attr_getdetachstate( const pthread_attr_t *attr, int *state ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || state == NULL )
        return EINVAL;

    *state = *attr & _THREAD_ATTR_DETACH_MASK;
    return 0;
}

int pthread_attr_getguardsize( const pthread_attr_t *, size_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getinheritsched( const pthread_attr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getschedparam( const pthread_attr_t *, struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getschedpolicy( const pthread_attr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getscope( const pthread_attr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getstack( const pthread_attr_t *, void **, size_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getstackaddr( const pthread_attr_t *, void ** ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_getstacksize( const pthread_attr_t *, size_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setdetachstate( pthread_attr_t *attr, int state ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || ( state & ~_THREAD_ATTR_DETACH_MASK ) )
        return EINVAL;

    *attr &= ~_THREAD_ATTR_DETACH_MASK;
    *attr |= state;
    return 0;
}

int pthread_attr_setguardsize( pthread_attr_t *, size_t ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setinheritsched( pthread_attr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setschedparam( pthread_attr_t *, const struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setschedpolicy( pthread_attr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setscope( pthread_attr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setstack( pthread_attr_t *, void *, size_t ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setstackaddr( pthread_attr_t *, void * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_attr_setstacksize( pthread_attr_t *, size_t ) noexcept {
    /* TODO */
    return 0;
}

/* Thread ID */

/* pthread is represented by dios task */
pthread_t pthread_self( void ) noexcept {
    __dios::FencedInterruptMask mask;
    return __dios_this_task();
}

int pthread_equal( pthread_t t1, pthread_t t2 ) noexcept {
    return t1 == t2;
}

/* Scheduler */
int pthread_getconcurrency( void ) noexcept {
    /* TODO */
    return 0;
}

int pthread_getcpuclockid( pthread_t, clockid_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_getschedparam( pthread_t, int *, struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_setconcurrency( int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_setschedparam( pthread_t, int, const struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_setschedprio( pthread_t, int ) noexcept {
    /* TODO */
    return 0;
}

/* Spin lock */

int pthread_spin_destroy( pthread_spinlock_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_spin_init( pthread_spinlock_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_spin_lock( pthread_spinlock_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_spin_trylock( pthread_spinlock_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_spin_unlock( pthread_spinlock_t * ) noexcept {
    /* TODO */
    return 0;
}

/* POSIX Realtime Extension - sched.h */
int sched_get_priority_max( int ) noexcept {
    /* TODO */
    return 0;
}

int sched_get_priority_min( int ) noexcept {
    /* TODO */
    return 0;
}

int sched_getparam( pid_t, struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int sched_getscheduler( pid_t ) noexcept {
    /* TODO */
    return 0;
}

int sched_rr_get_interval( pid_t, struct timespec * ) noexcept {
    /* TODO */
    return 0;
}

int sched_setparam( pid_t, const struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int sched_setscheduler( pid_t, int, const struct sched_param * ) noexcept {
    /* TODO */
    return 0;
}

int sched_yield( void ) noexcept {
    /* TODO */
    return 0;
}

int pthread_yield( void ) noexcept {
    return 0;
}
