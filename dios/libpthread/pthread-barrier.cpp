// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

using namespace __dios;

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
