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

    struct _DiOS_TLS ** threads = __dios_get_process_tasks();
    struct _DiOS_TLS * current_thread = __dios_get_task_handle();
    int cnt = __vm_obj_size( threads ) / sizeof( struct _DiOS_TLS * );
    pid_t pid = fork();

    if ( pid == 0 )
    {
        glob++;
        for ( int i = 0; i < cnt; ++i )
           if ( threads[ i ] != current_thread )
               threads[ i ]->_errno; /* ERROR */
    }
    else
    {
        void* s;
        pthread_join( tid, &s );
    }

}
