/* TAGS: min threads c++ */
#include <pthread.h>
#include <assert.h>

void *die( void *_ ) {
    return (void*)((size_t)42);
}

int main() {
    pthread_t t;
    int r = pthread_create( &t, 0, die, 0 );
    assert( r == 0 );
    void *st;
    pthread_join( t, &st );
    assert( ((size_t)st) == 42 );
    assert( 0 ); /* ERROR */
}

