/*
 * Pthread barrier
 * ===============
 *
 *  Test case of the implementation of a barrier for threads in the Pthread library
 *  provided by DiVinE.
 *
 *  *tags*: test, C++98
 *
 * Description
 * -----------
 *
 *  This program is a simple test case of the implementation of a barrier for threads in
 *  the Pthread library provided by DiVinE. This test is not complete, but it gives
 *  quite a strong evidence of its correctness.
 *
 * Parameters
 * ----------
 *
 *  - `RELEASE_COUNT`: a number of threads that must call `pthread_barrier_wait()` before
 *                     any of them successfully return from the call
 *                     (the third argument of the `pthread_barrier_init()` function)
 *  - `PASS_COUNT`: specifies how many times the barrier should perform the release of waiting threads
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm pthread_barrier.cpp
 *         $ divine verify -p assert pthread_barrier.bc -d
 *         $ divine verify -p safety pthread_barrier.bc -d
 *
 *  - playing with the parameters:
 *
 *         $ divine compile --llvm --cflags="-DRELEASE_COUNT=5 -DPASS_COUNT=3" pthread_barrier.cpp
 *         $ divine verify -p assert pthread_barrier.bc -d
 *         $ divine verify -p safety pthread_barrier.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang++ -lpthread -o pthread_barrier.exe pthread_barrier.cpp
 *       $ ./pthread_barrier.exe
 */


#include <pthread.h>
#include <assert.h>

#ifndef RELEASE_COUNT
#define RELEASE_COUNT   2
#endif

#ifndef PASS_COUNT
#define PASS_COUNT      2
#endif

#define NUM_OF_THREADS  ( RELEASE_COUNT * PASS_COUNT )

volatile unsigned count = 0;
volatile unsigned serial = 0;

pthread_mutex_t mutex;
pthread_barrier_t barrier1, barrier2;

void *thread( void* arg ) {
    pthread_mutex_lock( &mutex );
    count++;
    pthread_mutex_unlock( &mutex );

    int r = 0;
    if ( (r = pthread_barrier_wait( &barrier1 ))
            == PTHREAD_BARRIER_SERIAL_THREAD )
    {
        pthread_mutex_lock( &mutex );
        serial++;
        pthread_mutex_unlock( &mutex );
        // Barrier shouldn't release less then <RELEASE_COUNT> threads.
        // Well, it shouldn't wait for more threads either, but this is hard
        // to verify.
        assert( count >= RELEASE_COUNT );
    } else
        assert( r == 0 );

    // Wait for the "serial" thread of the previous barrier to make verification.
    r = pthread_barrier_wait( &barrier2 );
    assert( r == 0 || r == PTHREAD_BARRIER_SERIAL_THREAD );

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
