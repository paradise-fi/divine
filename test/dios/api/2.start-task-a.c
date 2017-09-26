#include <dios.h>
#include <stdlib.h>
#include <assert.h>

volatile int start;

void *getTls( _DiOS_TaskHandle id ) {
    return &id->data;
}

void routine( void * x ){
    while( !start );
    int *tls = getTls( __dios_get_task_handle() );
    *tls = 42;
}


int main() {
    int *tls = getTls( __dios_start_task( routine, NULL, 2 * sizeof( int ) ) );
    *tls = 0;
    start = 1;
    while( *tls != 42 );
    assert( 0 ); /* ERROR */
}
