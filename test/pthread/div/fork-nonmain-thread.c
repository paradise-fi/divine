/* TAGS: fork min threads c */
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

volatile int glob = 0;
volatile pid_t pid = -1;

void *thread( void *x )
{
    pid = fork();
    return 1; // disregarded, would crash&burn if it were exit(1)
}

int main()
{
    pthread_t tid;

    pthread_create( &tid, NULL, thread, NULL );

    void* s;
    int ret = pthread_join( tid, &s );

    assert( ret == 0 );
    assert( pid == 2 );
}
