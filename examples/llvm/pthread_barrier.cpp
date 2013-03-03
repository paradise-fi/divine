/*
 * This program is a simple test case of the implementation of the barrier in
 * Pthread library provided by DiVinE. This test is not complete, but it gives
 * quite a strong evidence of its correctness.
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] pthread_barrier.c
 *  $ divine verify -p assert pthread_barrier.bc [-d]
 * Execute with:
 *  $ clang++ [ < flags > ] -lpthread -o pthread_barrier.exe pthread_barrier.cpp
 *  $ ./pthread_barrier.exe
 */


#include <pthread.h>

#define RELEASE_COUNT   2
#define PASS_COUNT      2
#define NUM_OF_THREADS  ( RELEASE_COUNT * PASS_COUNT )

// For native execution.
#ifndef DIVINE
#include <cassert>
#endif

unsigned count = 0;
unsigned serial = 0;

pthread_mutex_t mutex;
pthread_barrier_t barrier1, barrier2;

void *thread( void* arg ) {
    pthread_mutex_lock( &mutex );
    count++;
    pthread_mutex_unlock( &mutex );

    if ( pthread_barrier_wait( &barrier1 ) == PTHREAD_BARRIER_SERIAL_THREAD ) {
        serial++;
        // Barrier shouldn't release less then <RELEASE_COUNT> threads.
        // Well, it shouldn't wait for more threads either, but this is hard
        // to verify.
        assert( count >= RELEASE_COUNT );
    }

    // Wait for the "serial" thread of the previous barrier to make verification.
    pthread_barrier_wait( &barrier2 );

    pthread_mutex_lock( &mutex );
    count--;
    pthread_mutex_unlock( &mutex );

    return NULL;
}

int main() {

    int i;
    pthread_t threads[NUM_OF_THREADS];
    pthread_barrier_init( &barrier1, NULL, RELEASE_COUNT );
    pthread_barrier_init( &barrier2, NULL, RELEASE_COUNT );
    pthread_mutex_init( &mutex, NULL );

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        pthread_create( &threads[i], 0, thread, NULL );
    }

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }

    assert( serial == PASS_COUNT ); // How many passes were actually made?
    return 0;
}
