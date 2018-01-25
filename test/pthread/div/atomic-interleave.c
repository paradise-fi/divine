/* TAGS: min threads c */
#include <assert.h>
#include <pthread.h>
volatile int shared = 0;

void *thread( void *x )
{
    while ( shared == 0 );
    assert( shared == 1 ); /* ERROR */
    return 0;
}

int main()
{
    pthread_t tid;
    pthread_create( &tid, NULL, thread, NULL );
    __sync_add_and_fetch( &shared, 1 );
    __sync_add_and_fetch( &shared, -1 );
    assert( shared == 0 );
    pthread_join( tid, NULL );
    return 0;
}
