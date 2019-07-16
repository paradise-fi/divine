/* TAGS: min threads c++ */
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>

int *mem = 0;

void *thread( void *x )
{
    assert( *mem == 0 );
}

void dtor() { sched_yield(); free( mem ); }

int main()
{
    if ( mem = malloc( sizeof( int ) ) )
    {
        *mem = 0;

        pthread_t tid;
        pthread_create( &tid, NULL, thread, NULL );
        atexit( dtor );
    }
}

