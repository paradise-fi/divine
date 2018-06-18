/* TAGS: min threads c */
#include <assert.h>
#include <pthread.h>

void *thread( void *x )
{
     return (void *)1;
}

int main()
{
    pthread_t detached[ 2 ];
    pthread_attr_t startDetach;
    pthread_attr_init( &startDetach );
    pthread_attr_setdetachstate( &startDetach, PTHREAD_CREATE_DETACHED );
    pthread_create( &detached[ 0 ], &startDetach, thread, NULL );
    pthread_create( &detached[ 1 ], NULL, thread, NULL );
    pthread_t tid;
    pthread_create( &tid, NULL, thread, NULL );
    pthread_detach( detached[ 1 ] );
    void *i = 0;
    pthread_join( tid, &i );
    assert( (int)i == 1 );
    pthread_attr_destroy( &startDetach );
    return 0;
}
