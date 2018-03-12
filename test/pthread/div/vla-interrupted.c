/* TAGS: min c */
#include <pthread.h>
#include <sys/divm.h>
#include <stdlib.h>

void *thread( void *zzz )
{
    int y;
    int *x = &y;
    {
        int vla[1 + (int) zzz];
        __dios_interrupt();
    }
    *x = 1;
    return 1;
}

int main( int argc, char **argv )
{
    pthread_t tid;
    pthread_create( &tid, NULL, thread, NULL );
    free( malloc( 1 ) );
    return 0;
}
