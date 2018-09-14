/* TAGS: min threads c */
#include <pthread.h>
#include <assert.h>
#include <errno.h>

int main() {
    pthread_barrier_t barrier;
    int r = pthread_barrier_init( &barrier, NULL, 1 );
    assert( r == 0 );
    r = pthread_barrier_wait( &barrier );
    assert( r == PTHREAD_BARRIER_SERIAL_THREAD );
    r = pthread_barrier_destroy( &barrier );
    assert( r == 0 );
    r = pthread_barrier_init( &barrier, NULL, 2 );
    assert( r == 0 );
}

