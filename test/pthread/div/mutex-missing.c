/* TAGS: min threads c */
#include <assert.h>
#include <pthread.h>

int shared = 0;

void *thread( void *x )
{
    ++ shared;
    return 0;
}

int main()
{
    pthread_t tid;
    pthread_create( &tid, NULL, thread, NULL );
    ++ shared;
    pthread_join( tid, NULL );
    assert( shared == 2 ); /* ERROR */
    return 0;
}
