// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

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

using namespace __dios;

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
