/*
 * Name
 * ====================
 *  Pthread mutex
 *
 * Category
 * ====================
 *  Test
 *
 * Short description
 * ====================
 *  This program is a simple test case for the implementation of the recursive
 *  mutex in Pthread library provided by DiVinE.
 *
 * Long description
 * ====================
 *  When recursive mutex is used, program usually becomes less vulnerable for
 *  deadlocks, but still number of unlocks have to match number of locks applied
 *  for the mutex to become available for other threads to acquire and also
 *  for final destruction. But when compiled with `-DBUG`, this rule is violated.
 *
 * Verification
 * ====================
 *     $ divine compile --llvm [--cflags=" < flags > "] pthread_mutex.c
 *     $ divine verify -p assert pthread_mutex.bc [-d]
 *
 * Execution
 * ====================
 *     $ clang [ < flags > ] -lpthread -o pthread_mutex.exe pthread_mutex.cpp
 *     $ ./pthread_mutex.exe
 *
 * Standard
 * ====================
 *  C99
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
