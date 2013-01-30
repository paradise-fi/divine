/*
 * This program is a simple test case for the implementation of
 * the Readers-Writer lock in Pthread library provided by DiVinE.
 * Reading and writing is interleaved in all (visible) possibilities
 * (currently also those unfair) and at some points safety is verified.
 *
 * Read lock ensures for reader, that reading consisting of possibly
 * more than one load will not be interleaved with writing. But when compiled
 * with macro BUG defined, read lock is disabled and hence safety is violated
 * (even thought writer still uses write lock).
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] pthread_rwlock.c
 *  $ divine verify -p assert pthread_rwlock.bc [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o pthread_rwlock.exe pthread_rwlock.cpp
 *  $ ./pthread_rwlock.exe
 */

#include <pthread.h>

// For native execution (in future we will provide cassert).
#ifndef DIVINE
#include "stdlib.h"
#include "assert.h"
#endif

#define NUM_OF_READERS     2
#define NUM_OF_WRITERS     2

#define INITIAL            1
#define EOF                3
// stream of exchanged data = INITIAL, INITIAL + 1, ... , EOF - 1, EOF

#define ERRNO( fun ) assert( !fun );

#ifndef BUG
#define VERIFY_RWLOCK
#endif

int global = INITIAL;
int writers = 0;
int readers = 0;
int ready_readers = 0;
pthread_mutex_t mutex; // mutex for readers
pthread_rwlock_t rwlock;

#ifdef VERIFY_RWLOCK
void check() {
    // ( writers != 0 ) => ( writers = 1 and readers = 0 )
    assert( !writers || ( writers == 1 && !readers ) );
}
#endif

void write() {
#ifdef VERIFY_RWLOCK
    ++writers;

    check();
    ++global;

    --writers;
#else
    ++global;
#endif
}

int read() {
#ifdef VERIFY_RWLOCK
    pthread_mutex_lock( &mutex );
    ++readers;
    pthread_mutex_unlock( &mutex );

    check();
    int local = global;

    pthread_mutex_lock( &mutex );
    --readers;
    pthread_mutex_unlock( &mutex );

    return local;
#else
    return global;
#endif
}

void *reader( void *arg ) {
    (void) arg;

    // read initial value
    ERRNO( pthread_rwlock_tryrdlock( &rwlock ) ) // shouldn't fail
    int local = read();
    assert( local == INITIAL );
    ERRNO( pthread_rwlock_unlock( &rwlock ) )

    // here the thread would process initial value

    pthread_mutex_lock( &mutex );
    ++ready_readers;
    pthread_mutex_unlock( &mutex );

    while ( local != EOF ) {

        // Avoids infinite state space. When fairness will be
        // implemented, this become unecessary.
        while ( local == global );

#ifndef BUG
        ERRNO( pthread_rwlock_rdlock( &rwlock ) )
#endif

        local = read();
        // Here the thread would continue with reading and processing of loaded data
        // (which won't change meanwhile, if the read lock is obtained).
        assert( local == read() ); // fails with macro BUG defined

#ifndef BUG
        ERRNO( pthread_rwlock_unlock( &rwlock ) )
#endif
    }

    return NULL;
}

void *writer( void *arg ) {
    (void) arg;
    int local;

    // wait for all readers to read initial value
    while ( ready_readers < NUM_OF_READERS );

    do {
        ERRNO( pthread_rwlock_wrlock( &rwlock ) )
        local = read();
        if ( local != EOF ) {
            // here the thread would produce next value
            write();
        }
        ERRNO( pthread_rwlock_unlock( &rwlock ) )
    } while ( local != EOF );

    return NULL;
}

int main() {
    int i;
    pthread_t readers[NUM_OF_READERS];
    pthread_t writers[NUM_OF_WRITERS];

    pthread_mutex_init( &mutex, NULL );
    pthread_rwlock_init( &rwlock, NULL );

    // create threads
    for ( i = 0; i < NUM_OF_READERS; i++ )
        pthread_create( &readers[i], NULL, reader, NULL );

    for ( i = 0; i < NUM_OF_WRITERS; i++ )
        pthread_create( &writers[i], NULL, writer, NULL );

    // join threads
    for ( i = 0; i < NUM_OF_READERS; i++ )
        pthread_join( readers[i], NULL );

    for ( i = 0; i < NUM_OF_WRITERS; i++ )
        pthread_join( writers[i], NULL );

    assert( global == EOF );
    pthread_mutex_destroy( &mutex );
    pthread_rwlock_destroy( &rwlock );
    return 0;
}
