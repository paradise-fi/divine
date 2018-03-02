/* TAGS: min threads c */
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

volatile int glob = 0;

void *thread( void *x )
{
    while ( !glob );
    return 1;
}

int main()
{
    pthread_t tid;

    pthread_create( &tid, NULL, thread, NULL );

    struct __dios_tls **threads = __dios_get_process_tasks();
    struct __dios_tls *current_thread = __dios_this_task();
    int cnt = __vm_obj_size( threads ) / sizeof( struct __dios_tls * );
    pid_t pid = fork();

    if ( pid == 0 )
    {
        glob++;
        for ( int i = 0; i < cnt; ++i )
            if ( threads[ i ] != current_thread )
                threads[ i ]->__errno; /* ERROR */
    }
    else
    {
        void* s;
        pthread_join( tid, &s );
    }

}
