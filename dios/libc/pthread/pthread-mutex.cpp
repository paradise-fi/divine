// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

namespace __dios
{

/* Mutex */

/*
  pthread_mutex_t representation:
  -----------------------------------------------------------------------------------------------
 | *free* | initialized?: 1 bit | type: 2 bits | lock counter: 8 bits |
 (thread_id + 1): 16 bits |
  -----------------------------------------------------------------------------------------------
  */

static int _mutex_adjust_count( pthread_mutex_t *mutex, int adj ) noexcept {
    int count = mutex->__lockCounter;
    count += adj;
    if ( count >= ( 1 << 28 ) || count < 0 )
        return EAGAIN;

    mutex->__lockCounter = count;
    return 0;
}

static bool _check_deadlock( pthread_mutex_t *mutex, _PThread &tid ) noexcept
{
    // note: the cycle is detected first time it occurs, therefore it must go
    // through this mutex, for this reason, we don't need to keep closed set of
    // visited threads and mutexes.
    // Furthermore, if the cycle is re-detected it is already marked in
    // 'deadlocked' attribute of Thread struct.

    while ( mutex && mutex->__owner )
    {
        _PThread *owner = &getThread( mutex->__owner );
        if ( owner == &tid )
        {
            __dios_fault( _VM_Fault::_VM_F_Locking, "deadlock: circular wait on a mutex" );
            return tid.deadlocked = owner->deadlocked = true;
        }
        if ( owner->deadlocked )
        {
            __dios_fault( _VM_Fault::_VM_F_Locking, "deadlock: waiting for a deadlocked thread" );
            return tid.deadlocked = true;
        }
        if ( !owner->running )
        {
            __dios_fault( _VM_Fault::_VM_F_Locking, "deadlock: mutex locked by a dead thread" );
            return tid.deadlocked = true;
        }
        mutex = owner->waiting_mutex;
    }
    return false;
}

static bool _mutex_can_lock( pthread_mutex_t *mutex, _PThread &thr ) noexcept {
    return !mutex->__owner || ( &getThread( mutex->__owner ) == &thr );
}

int _mutex_lock( __dios::FencedInterruptMask &mask, pthread_mutex_t *mutex, bool wait ) noexcept
{
    __dios_task gtid = __dios_this_task();
    _PThread &thr = getThread( gtid );

    if ( mutex == NULL ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( mutex->__owner && &getThread( mutex->__owner ) == &thr ) {
        // already locked by this thread
        assert( mutex->__lockCounter ); // count should be > 0
        if ( mutex->__type != PTHREAD_MUTEX_RECURSIVE ) {
            if ( mutex->__type == PTHREAD_MUTEX_ERRORCHECK )
                return EDEADLK;
            else
                __dios_fault( _VM_Fault::_VM_F_Locking, "deadlock: double lock on a nonrecursive mutex" );
        }
    }

    if ( !_mutex_can_lock( mutex, thr ) ) {
        assert( mutex->__lockCounter ); // count should be > 0
        if ( !wait )
            return EBUSY;
    }

    // mark waiting now for wait cycle detection
    // note: waiting should not be set in case of unsuccessfull try-lock
    // (cycle in try-lock is not deadlock, although it might be livelock)
    // so it must be here after return EBUSY
    thr.waiting_mutex = mutex;
    bool can_lock = _mutex_can_lock( mutex, thr );

    if ( !can_lock )
        mask.without( [] { __dios_reschedule(); } );

    if ( !_mutex_can_lock( mutex, thr ) )
    {
        if ( _check_deadlock( mutex, thr ) )
            __dios_suspend();
        else
            __vm_cancel();
    }

    thr.waiting_mutex = NULL;

    // try to increment lock counter
    int err = _mutex_adjust_count( mutex, 1 );
    if ( err )
        return err;

    // lock the mutex
    acquireThread( thr );
    mutex->__owner = gtid;

    return 0;
}

}

using namespace __dios;

int pthread_mutex_destroy( pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( mutex == NULL )
        return EINVAL;

    if ( mutex->__owner ) {
        // mutex is locked
        if ( mutex->__type == PTHREAD_MUTEX_ERRORCHECK )
            return EBUSY;
        else
            __dios_fault( _VM_Fault::_VM_F_Locking, "destroying a locked mutex" );
    }
    pthread_mutex_t uninit;
    *mutex = uninit;
    return 0;
}

int pthread_mutex_init( pthread_mutex_t *mutex, const pthread_mutexattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( mutex == NULL )
        return EINVAL;

    /* bitfield initializers can be only used as such, that is, initializers,
     * *not* as a right-hand side of an assignment to uninitialised memory */
    memset( mutex, 0, sizeof( pthread_mutex_t ) );
    pthread_mutex_t init = PTHREAD_MUTEX_INITIALIZER;
    *mutex = init;

    if ( attr )
        mutex->__type = attr->__type;
    else
        mutex->__type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutex_lock( pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;
    return _mutex_lock( mask, mutex, 1 );
}

int pthread_mutex_trylock( pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;
    return _mutex_lock( mask, mutex, 0 );
}

int pthread_mutex_unlock( pthread_mutex_t *mutex ) noexcept {
    __dios::FencedInterruptMask mask;
    _PThread &thr = getThread();

    if ( mutex == NULL ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( mutex->__owner == nullptr ) {
        if ( mutex->__type == PTHREAD_MUTEX_NORMAL )
            __dios_fault( _VM_Fault::_VM_F_Locking, "mutex already unlocked" );
        else
            return EPERM;
    }

    if ( mutex->__owner != __dios_this_task() ) {
        // mutex is not locked or it is already locked by another thread
        assert( mutex->__lockCounter ); // count should be > 0
        if ( mutex->__type == PTHREAD_MUTEX_NORMAL )
            __dios_fault( _VM_Fault::_VM_F_Locking, "mutex locked by another thread" );
        else
            return EPERM; // recursive mutex can also detect
    }

    int r = _mutex_adjust_count( mutex, -1 );
    assert( r == 0 );
    if ( !mutex->__lockCounter ) {
        releaseThread( getThread( mutex->__owner ) );
        mutex->__owner = nullptr; // unlock if count == 0

    }
    return 0;
}

int pthread_mutex_getprioceiling( const pthread_mutex_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutex_setprioceiling( pthread_mutex_t *, int, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutex_timedlock( pthread_mutex_t *mutex, const struct timespec *abstime ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( abstime == NULL || abstime->tv_nsec < 0 || abstime->tv_nsec >= MILLIARD ) {
        return EINVAL;
    }

    // Consider both scenarios, one in which the mutex could not be locked
    // before the specified timeout expired, and the other in which
    // pthread_mutex_timedlock behaves basically the same as pthread_mutex_lock.
    if ( __vm_choose( 2 ) ) {
        return pthread_mutex_lock( mutex );
    } else {
        return ETIMEDOUT;
    }
}

/* Mutex attributes */

/*
  pthread_mutexattr_t representation:
  -----------------------------
 | *free* | mutex type: 2 bits |
  -----------------------------
*/

int pthread_mutexattr_destroy( pthread_mutexattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    int uninit;
    attr->__type = uninit;
    return 0;
}

int pthread_mutexattr_init( pthread_mutexattr_t *attr ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL )
        return EINVAL;

    attr->__type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_gettype( const pthread_mutexattr_t *attr, int *value ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || value == NULL )
        return EINVAL;

    *value = attr->__type & _MUTEX_ATTR_TYPE_MASK;
    return 0;
}

int pthread_mutexattr_getprioceiling( const pthread_mutexattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_getprotocol( const pthread_mutexattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_getpshared( const pthread_mutexattr_t *, int * ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_settype( pthread_mutexattr_t *attr, int value ) noexcept {
    __dios::FencedInterruptMask mask;

    if ( attr == NULL || ( value & ~_MUTEX_ATTR_TYPE_MASK ) )
        return EINVAL;

    attr->__type = value;
    return 0;
}

int pthread_mutexattr_setprioceiling( pthread_mutexattr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_setprotocol( pthread_mutexattr_t *, int ) noexcept {
    /* TODO */
    return 0;
}

int pthread_mutexattr_setpshared( pthread_mutexattr_t *, int ) noexcept {
    /* TODO */
    return 0;
}
