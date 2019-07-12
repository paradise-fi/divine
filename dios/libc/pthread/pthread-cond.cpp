// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

using namespace __dios;

/* Conditional variables */

/*
  pthread_cond_t representation:

    { .mutex: address of associated mutex
      .counter: number of waiting threads: 16 bits
      .initialized: 1 bit
      ._pad: 15 bits
    }
*/

int pthread_cond_destroy( pthread_cond_t *cond ) noexcept
{
    __dios::FencedInterruptMask mask;

    int r = _destroy_cond_or_barrier( cond );
    if ( r == 0 ) {
        pthread_mutex_t *uninit;
        cond->__mutex = uninit;
    }
    return r;
}

int pthread_cond_init( pthread_cond_t *cond, const pthread_condattr_t * /* TODO */ ) noexcept
{
    __dios::FencedInterruptMask mask;

    if ( cond == NULL )
        return EINVAL;

    *cond = ( pthread_cond_t )PTHREAD_COND_INITIALIZER;
    return 0;
}

int pthread_cond_signal( pthread_cond_t *cond ) noexcept
{
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

int pthread_cond_wait( pthread_cond_t *cond, pthread_mutex_t *mutex ) noexcept
{
    __dios::FencedInterruptMask mask;

    _PThread &thread = getThread();

    if ( cond == NULL )
        return EINVAL;

    if ( mutex == NULL ) {
        return EINVAL;
    }

    if ( mutex->__owner != pthread_self() ) {
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

int pthread_cond_timedwait( pthread_cond_t *cond, pthread_mutex_t *mutex,
                            const struct timespec *abstime ) noexcept
{
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
int pthread_condattr_destroy( pthread_condattr_t * ) noexcept
{
    /* TODO */
    return 0;
}

int pthread_condattr_getclock( const pthread_condattr_t *, clockid_t * ) noexcept
{
    /* TODO */
    return 0;
}

int pthread_condattr_getpshared( const pthread_condattr_t *, int * ) noexcept
{
    /* TODO */
    return 0;
}

int pthread_condattr_init( pthread_condattr_t * ) noexcept
{
    /* TODO */
    return 0;
}

int pthread_condattr_setclock( pthread_condattr_t *, clockid_t ) noexcept
{
    /* TODO */
    return 0;
}

int pthread_condattr_setpshared( pthread_condattr_t *, int ) noexcept
{
    /* TODO */
    return 0;
}
