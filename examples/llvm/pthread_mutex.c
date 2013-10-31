/*
 * Pthread mutex
 * =============
 *
 *  This program is a simple test case for the implementation of the recursive
 *  mutex in the Pthread library provided by DiVinE.
 *
 *  *tags*: test, C99
 *
 * Description
 * -----------
 *
 *  When a recursive mutex is used, program usually becomes less vulnerable to
 *  deadlock, but still the number of unlocks has to match the number of locks applied
 *  for the mutex to become available for other threads to acquire and also
 *  for the final destruction. But when compiled with `-DBUG`, this rule is violated.
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the algorithm is incorrect and violates the safety property
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm pthread_mutex.c
 *         $ divine verify -p assert pthread_mutex.bc -d
 *         $ divine verify -p deadlock pthread_mutex.bc -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-DBUG" pthread_mutex.c
 *         $ divine verify -p assert pthread_mutex.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang -lpthread -o pthread_mutex.exe pthread_mutex.cpp
 *       $ ./pthread_mutex.exe
 */

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

void *other(void *x) {
    pthread_mutex_t *mutex = (pthread_mutex_t*) x;
    pthread_mutex_lock( mutex );
    critical();
    pthread_mutex_unlock( mutex );
    return NULL;
}

int main() {
    pthread_t id;
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;

    pthread_mutexattr_init( &attr );
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );

    pthread_mutex_init( &mutex, &attr );

    void *result;
    pthread_create( &id, NULL, other, &mutex );

    pthread_mutex_lock( &mutex );

    pthread_mutex_lock( &mutex );
    pthread_mutex_unlock( &mutex );

    critical();

#ifndef BUG
    // number of unlocks should match number of locks
    pthread_mutex_unlock( &mutex );
#endif

#ifndef __divine__
    printf( "Waiting for %d.\n", id );
#endif

    pthread_join( id, &result );
    pthread_mutex_destroy( &mutex ); // undefined if mutex is locked (DiVinE raises assertion violation)
    return 0;
}
