/* TAGS: threads c */
#include <pthread.h>
#include <assert.h>
#include <errno.h>

pthread_cond_t cond;

int main() {
    int r = pthread_cond_init( &cond, NULL );
    assert( r == 0 );
    r = pthread_cond_broadcast( &cond );
    assert( r == 0 );
    r = pthread_cond_destroy( &cond );
    assert( r == 0 );
    r = pthread_cond_init( &cond, NULL );
    assert( r == 0 );
}
