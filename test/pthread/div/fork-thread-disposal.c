/* TAGS: fork min threads c */
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

volatile int glob = 0;

void *thread( void *x )
{
    while ( !glob );
    assert( glob == 1 );
    return 1;
}

int main()
{
    pthread_t tid;

    pthread_create( &tid, NULL, thread, NULL );

    pid_t pid = fork();

    if ( pid == 0 )
    {
        glob = 2;
    }
    else
    {
        glob = 1;
        void* s;
        pthread_join( tid, &s );
    }
}
