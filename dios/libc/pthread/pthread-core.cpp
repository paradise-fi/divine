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

static bool _canceled() noexcept {
    return getThread().cancelled;
}

template < typename Cond >
__attribute__( ( __always_inline__, __flatten__ ) )
static void waitOrCancel( __dios::FencedInterruptMask &mask, Cond cond ) noexcept
{
    if ( _canceled() )
        _cancel( mask );
    else
        wait( mask, cond );
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

/* Conditional variables */

/*
  pthread_cond_t representation:

    { .mutex: address of associated mutex
      .counter: number of waiting threads: 16 bits
      .initialized: 1 bit
      ._pad: 15 bits
    }
*/

template < typename CondOrBarrier > int _cond_adjust_count( CondOrBarrier *cond, int adj ) noexcept {
    int count = cond->__counter;
    count += adj;
    assert( count < ( 1 << 16 ) );
    assert( count >= 0 );

    cond->__counter = count;
    return count;
}

template < typename CondOrBarrier > int _destroy_cond_or_barrier( CondOrBarrier *cond ) noexcept {

    if ( cond == NULL )
        return EINVAL;

    // make sure that no thread is waiting on this condition
    // (probably better alternative when compared to: return EBUSY)
    assert( cond->__counter == 0 );

    CondOrBarrier uninit;
    *cond = uninit;
    return 0;
}

int pthread_cond_destroy( pthread_cond_t *cond ) noexcept {
    __dios::FencedInterruptMask mask;

    int r = _destroy_cond_or_barrier( cond );
    if ( r == 0 ) {
        pthread_mutex_t *uninit;
        cond->__mutex = uninit;
    }
    return r;
}

int pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t * /* TODO: cond. attributes */ ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( cond == NULL )
        return EINVAL;

    *cond = ( pthread_cond_t )PTHREAD_COND_INITIALIZER;
    return 0;
}

template < typename > static inline SleepingOn _sleepCond() noexcept;
template <> inline SleepingOn _sleepCond< pthread_barrier_t >() noexcept {
    return Barrier;
}
template <> inline SleepingOn _sleepCond< pthread_cond_t >() noexcept {
    return Condition;
}

static inline bool _eqSleepTrait( _PThread &t, pthread_cond_t *cond ) noexcept {
    return t.condition == cond;
}

static inline bool _eqSleepTrait( _PThread &t, pthread_barrier_t *bar ) noexcept {
    return t.barrier == bar;
}

template< bool broadcast, typename CondOrBarrier >
static int _cond_signal( CondOrBarrier *cond ) noexcept {
    if ( cond == NULL )
        return EINVAL;

    int count = cond->__counter;
    if ( count ) { // some threads are waiting for condition
        int waiting = 0, wokenup = 0, choice;

        if ( !broadcast ) {
            /* non-determinism */
            choice = __vm_choose( ( 1 << count ) - 1 );
        }

        iterateThreads( [&]( pthread_t tid ) {

            _PThread &thread = getThread( tid );

            if ( thread.sleeping == _sleepCond< CondOrBarrier >() && _eqSleepTrait( thread, cond ) ) {
                ++waiting;
                if ( broadcast || ( ( choice + 1 ) & ( 1 << ( waiting - 1 ) ) ) ) {
                    // wake up the thread
                    thread.sleeping = NotSleeping;
                    thread.condition = NULL;
                    ++wokenup;
                }
            }
        } );

        assert( count == waiting );

        _cond_adjust_count( cond, -wokenup );
    }
    return 0;
}

int pthread_cond_signal( pthread_cond_t *cond ) noexcept {
    __dios::FencedInterruptMask mask;
    int r = _cond_signal< false >( cond );
    if ( r == 0 && cond->__counter == 0 )
        cond->__mutex = NULL; // break binding between cond. variable and mutex
    return r;
}

int pthread_cond_broadcast( pthread_cond_t *cond ) noexcept {
    __dios::FencedInterruptMask mask;
    int r = _cond_signal< true >( cond );
    if ( r == 0 && cond->__counter == 0 )
        cond->__mutex = NULL; // break binding between cond. variable and mutex
    return r;
}

int pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;

    _PThread &thread = getThread();

    if ( cond == NULL )
        return EINVAL;

    if ( mutex == NULL ) {
        return EINVAL;
    }

    if ( mutex->__owner != &thread ) {
        // mutex is not locked or it is already locked by another thread
        return EPERM;
    }

    if ( __vm_choose( 2 ) )
        return 0; // simulate spurious wakeup

    // It is allowed to have one mutex associated with more than one conditional
    // variable. On the other hand, using more than one mutex for one
    // conditional variable results in undefined behaviour.
    if ( cond->__mutex != NULL && cond->__mutex != mutex )
        __dios_fault( _VM_Fault::_VM_F_Locking, "Attempted to wait on condition variable using other mutex "
                                        "that already in use by this condition variable" );
    cond->__mutex = mutex;

    // fall asleep
    thread.setSleeping( cond );

    _cond_adjust_count( cond, 1 ); // one more thread is waiting for this condition
    pthread_mutex_unlock( mutex ); // unlock associated mutex

    // sleeping
    waitOrCancel( mask, [&] { return thread.sleeping == Condition; } );

    // try to lock mutex which was associated to the cond. variable by this
    // thread
    // (not the one from current binding, which may have changed)
    _mutex_lock( mask, mutex, 1 );
    return 0;
}

int pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= MILLIARD )
        return EINVAL;

    // Consider both scenarios, one in which the time specified by abstime has
    // passed
    // before the function could finish, and the other in which
    // pthread_cond_timedwait behaves basically the same as pthread_cond_wait.
    if ( __vm_choose( 2 ) ) {
        return pthread_cond_wait( cond, mutex );
    } else {
        return ETIMEDOUT;
    }
}

/* Attributes of conditional variables */
int pthread_condattr_destroy( pthread_condattr_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_getclock( const pthread_condattr_t *, clockid_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_getpshared( const pthread_condattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_init( pthread_condattr_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_setclock( pthread_condattr_t *, clockid_t ) noexcept {
    /* TODO */
    return 0;
}

int pthread_condattr_setpshared( pthread_condattr_t *, int ) noexcept {
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

/* Readers-Writer lock */

namespace __dios
{

static int _rlock_adjust_count( _ReadLock *rlock, int adj ) noexcept {
    int count = rlock->__count;
    count += adj;
    if ( count < 0 )
        return EAGAIN;
    rlock->__count = count;
    return 0;
}

static _ReadLock *_get_rlock( pthread_rwlock_t *rwlock, _PThread &tid, _ReadLock **prev = nullptr ) noexcept {
    _ReadLock *rlock = rwlock->__rlocks;
    _ReadLock *_prev = nullptr;

    while ( rlock && ( rlock->__owner != &tid ) ) {
        _prev = rlock;
        rlock = rlock->__next;
    }

    if ( rlock && prev )
        *prev = _prev;

    return rlock;
}

static _ReadLock *_create_rlock( pthread_rwlock_t *rwlock, _PThread &tid ) noexcept {
    _ReadLock *rlock = reinterpret_cast< _ReadLock * >( __vm_obj_make( sizeof( _ReadLock ) ) );
    acquireThread( tid );
    rlock->__owner = &tid;
    rlock->__count = 1;
    rlock->__next = rwlock->__rlocks;
    rwlock->__rlocks = rlock;
    return rlock;
}

static bool _rwlock_can_lock( pthread_rwlock_t *rwlock, bool writer ) noexcept {
    return !rwlock->__wrowner && !( writer && rwlock->__rlocks );
}

static int _rwlock_lock( __dios::FencedInterruptMask &mask, pthread_rwlock_t *rwlock, bool shouldwait, bool writer ) noexcept {
    _PThread &thr = getThread();

    if ( rwlock == nullptr ) {
        return EINVAL; // rwlock does not refer to an initialized rwlock object
    }

    if ( rwlock->__wrowner == &thr )
        // thread already owns write lock
        return EDEADLK;

    _ReadLock *rlock = _get_rlock( rwlock, thr );
    // existing read lock implies read lock counter having value more than zero
    assert( !rlock || rlock->__count );

    if ( writer && rlock )
        // thread already owns read lock
        return EDEADLK;

    if ( !_rwlock_can_lock( rwlock, writer ) ) {
        if ( !shouldwait )
            return EBUSY;
    }
    wait( mask, [&] { return !_rwlock_can_lock( rwlock, writer ); } );

    if ( writer ) {
        acquireThread( thr );
        rwlock->__wrowner = &thr;
    }
    else {
        if ( !rlock )
            rlock = _create_rlock( rwlock, thr );
        else {
            int err = _rlock_adjust_count( rlock, 1 );
            if ( err )
                return err;
        }
    }
    return 0;
}

}

int pthread_rwlock_destroy( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( rwlock == NULL )
        return EINVAL;

    if ( rwlock->__wrowner || rwlock->__rlocks )
        // rwlock is locked
        return EBUSY;

    pthread_rwlock_t uninit;
    *rwlock = uninit;
    return 0;
}

int pthread_rwlock_init( pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( rwlock == NULL )
        return EINVAL;

    rwlock->__processShared = attr && bool( *attr & _RWLOCK_ATTR_SHARING_MASK );
    rwlock->__wrowner = nullptr;
    rwlock->__rlocks = nullptr;
    return 0;
}

int pthread_rwlock_rdlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, true, false );
}

int pthread_rwlock_wrlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, true, true );
}

int pthread_rwlock_tryrdlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, false, false );
}

int pthread_rwlock_trywrlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;
    return _rwlock_lock( mask, rwlock, false, true );
}

int pthread_rwlock_unlock( pthread_rwlock_t *rwlock ) noexcept {
    __dios::FencedInterruptMask mask;

    _PThread &thr = getThread();

    if ( rwlock == NULL ) {
        return EINVAL; // rwlock does not refer to an initialized rwlock object
    }

    _ReadLock *rlock, *prev;
    rlock = _get_rlock( rwlock, thr, &prev );
    // existing read lock implies read lock counter having value more than zero
    assert( !rlock || rlock->__count );

    if ( rwlock->__wrowner != &thr && !rlock ) {
        // the current thread does not own the read-write lock
        return EPERM;
    }

    if ( rwlock->__wrowner == &thr ) {
        assert( !rlock );
        // release write lock
        releaseThread( *rwlock->__wrowner );
        rwlock->__wrowner = nullptr;
    } else {
        // release read lock
        _rlock_adjust_count( rlock, -1 );
        if ( rlock->__count == 0 ) {
            // no more read locks are owned by this thread
            if ( prev )
                prev->__next = rlock->__next;
            else
                rwlock->__rlocks = rlock->__next;
            releaseThread( *rlock->__owner );
            __vm_obj_free( rlock );
        }
    }
    return 0;
}

/* Readers-Write lock attributes */

/*
  pthread_rwlockattr_t representation:
     -------------------------
    | *free* | sharing: 1 bit |
     -------------------------
*/

int pthread_rwlockattr_destroy( pthread_rwlockattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    pthread_rwlockattr_t uninit;
    *attr = uninit;
    return 0;
}

int pthread_rwlockattr_getpshared( const pthread_rwlockattr_t *attr, int *pshared ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || pshared == NULL )
        return EINVAL;

    *pshared = *attr & _RWLOCK_ATTR_SHARING_MASK;
    return 0;
}

int pthread_rwlockattr_init( pthread_rwlockattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    *attr = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_rwlockattr_setpshared( pthread_rwlockattr_t *attr, int pshared ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || ( pshared & ~_RWLOCK_ATTR_SHARING_MASK ) )
        return EINVAL;

    *attr &= ~_RWLOCK_ATTR_SHARING_MASK;
    *attr |= pshared;
    return 0;
}

/* Barrier */

/*
  pthread_barrier_t representation:

   { .counter:
     initialized: 1 bit
     .nthreads: the number of threads to synchronize: 15 bits
   }
*/

int pthread_barrier_destroy( pthread_barrier_t *barrier ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( barrier == NULL )
        return EINVAL;

    int r = _destroy_cond_or_barrier( barrier );
    if ( r == 0 ) {
        int uninit;
        barrier->__nthreads = uninit;
    }
    return r;
}

int pthread_barrier_init(
        pthread_barrier_t *barrier,
        const pthread_barrierattr_t * /* TODO: barrier attributes */,
        unsigned count ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( count == 0 || barrier == NULL )
        return EINVAL;

    // Set the number of threads that must call pthread_barrier_wait() before
    // any of them successfully return from the call.
    *barrier = ( pthread_barrier_t ){ .__nthreads = uint16_t( count ), .__counter = 0 };
    return 0;
}

int pthread_barrier_wait( pthread_barrier_t *barrier ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( barrier == NULL )
        return EINVAL;

    int ret = 0;
    int counter = barrier->__counter;
    int release_count = barrier->__nthreads;

    if ( ( counter + 1 ) == release_count ) {
        _cond_signal< true >( barrier );
        ret = PTHREAD_BARRIER_SERIAL_THREAD;
    } else {
        // WARNING: this is not immune from spurious wakeup.
        // As of this writing, spurious wakeup is not implemented (and probably
        // never will).

        // fall asleep
        _PThread &thread = getThread();
        thread.setSleeping( barrier );

        _cond_adjust_count( barrier, 1 ); // one more thread is blocked on this barrier

        // sleeping
        waitOrCancel( mask, [&] { return thread.sleeping == Barrier; } );
    }

    return ret;
}

/* Barrier attributes */
int pthread_barrierattr_destroy( pthread_barrierattr_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_barrierattr_getpshared( const pthread_barrierattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_barrierattr_init( pthread_barrierattr_t * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_barrierattr_setpshared( pthread_barrierattr_t *, int ) noexcept {
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
