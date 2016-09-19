#include <pthread.h>
#include <divine.h>

void *thread( void *zzz )
{
    int y;
    int *x = &y;
    {
        int vla[1 + (int) zzz];
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Interrupted, _VM_CF_Interrupted );
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
