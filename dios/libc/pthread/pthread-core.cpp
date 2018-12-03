// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

namespace __dios
{

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

int pthread_create( pthread_t *ptid, const pthread_attr_t *attr, void *( *entry )( void * ), void *arg ) noexcept {
    __dios::FencedInterruptMask mask;

    // test input arguments
    __dios_assert( entry );
    if ( ptid == NULL || entry == NULL )
        return EINVAL;

    // create new thread and pass arguments to the entry wrapper
    Entry *args = static_cast< Entry * >( __vm_obj_make( sizeof( Entry ) ) );
    args->entry = entry;
    args->arg = arg;
    auto tid = __dios_start_task( __pthread_entry, static_cast< void * >( args ), 0 );
    // init thread here, before it has first chance to run
    __init_thread( tid, attr == nullptr ? PTHREAD_CREATE_JOINABLE : *attr );
    *ptid = tid;

    return 0;
}

void pthread_exit( void *result ) noexcept {
    __dios::FencedInterruptMask mask;

    auto gtid = __dios_this_task();
    _PThread &thread = getThread( gtid );
    thread.result = result;

    if ( thread.is_main )
        iterateThreads( [&]( __dios_task tid ) {
                _pthread_join( mask, tid, nullptr ); // joining self is detected and ignored
            } );

    _run_cleanup_handlers();
    _clean_and_become_zombie( mask, gtid );
}

int pthread_join( pthread_t gtid, void **result ) noexcept {
    __dios::FencedInterruptMask mask;
    return _pthread_join( mask, gtid, result );
}

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

/* Thread cancellation */
int pthread_setcancelstate( int state, int *oldstate ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( state & ~0x1 )
        return EINVAL;

    _PThread &thread = getThread();
    if ( oldstate )
        *oldstate = thread.cancel_state;
    thread.cancel_state = state & 1;
    return 0;
}

int pthread_setcanceltype( int type, int *oldtype ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( type & ~0x1 )
        return EINVAL;

    _PThread &thread = getThread();
    if ( oldtype )
        *oldtype = thread.cancel_type;
    thread.cancel_type = type & 1;
    return 0;
}

int pthread_cancel( pthread_t gtid ) noexcept {
    __dios::FencedInterruptMask mask;

    _PThread *thread = nullptr;
    {
        __dios::DetectFault f( _VM_F_Memory );
        thread = &getThread( gtid );
        if ( f.triggered() )
            return ESRCH;
    }

    if ( thread->cancel_state == PTHREAD_CANCEL_ENABLE )
        thread->cancelled = true;

    return 0;
}

void pthread_testcancel( void ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( _canceled() )
        _cancel( mask );
}

void pthread_cleanup_push( void ( *routine )( void * ), void *arg ) noexcept {
    __dios::FencedInterruptMask mask;

    assert( routine != NULL );

    _PThread &thread = getThread();
    CleanupHandler *handler = reinterpret_cast< CleanupHandler * >(
            __vm_obj_make( sizeof( CleanupHandler ) ) );
    handler->routine = routine;
    handler->arg = arg;
    handler->next = thread.cleanup_handlers;
    thread.cleanup_handlers = handler;
}

void pthread_cleanup_pop( int execute ) noexcept {
    __dios::FencedInterruptMask mask;

    _PThread &thread = getThread();
    CleanupHandler *handler = thread.cleanup_handlers;
    if ( handler != NULL ) {
        thread.cleanup_handlers = handler->next;
        if ( execute ) {
            handler->routine( handler->arg );
        }
        __vm_obj_free( handler );
    }
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
