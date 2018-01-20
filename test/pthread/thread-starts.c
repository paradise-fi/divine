/* TAGS: min threads c */
#include <pthread.h>
#include <assert.h>

void *die( void *_ ) {
    assert( 0 ); /* ERROR */
    return 0;
}

int main() {
    pthread_t t;
    int r = pthread_create( &t, 0, die, 0 );
    assert( r == 0 );
    pthread_join( t, 0 );
}

