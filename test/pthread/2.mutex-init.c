#include <pthread.h>
#include <assert.h>
#include <errno.h>

int main() {
    pthread_mutex_t mtx;
    int r = pthread_mutex_init( &mtx, NULL );
    assert( r == 0 );
    r = pthread_mutex_init( &mtx, NULL );
    assert( r == EBUSY );
    r = pthread_mutex_destroy( &mtx );
    assert( r == 0 );
    r = pthread_mutex_init( &mtx, NULL );
    assert( r == 0 );
}
