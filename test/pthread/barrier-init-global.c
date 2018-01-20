/* TAGS: threads c */
#include <pthread.h>
#include <assert.h>
#include <errno.h>

pthread_barrier_t barrier;

int main() {
    int r = pthread_barrier_init( &barrier, NULL, 2 );
    assert( r == 0 );
    r = pthread_barrier_init( &barrier, NULL, 2 );
    assert( r == EBUSY );
    r = pthread_barrier_destroy( &barrier );
    assert( r == 0 );
    r = pthread_barrier_init( &barrier, NULL, 2 );
    assert( r == 0 );
}

