#include <dios.h>
#include <stdlib.h>
#include <assert.h>

volatile int start;

void *getTls( _DiOS_ThreadId id ) {
    return ( char * ) id + _DiOS_TLS_Reserved;
}

void routine( void * x ){
    while( !start );
    int *tls = getTls( __dios_get_thread_id() );
    *tls = 42;
}


int main() {
    int *tls = getTls( __dios_start_thread( routine, NULL, 2 * sizeof( int ) ) );
    *tls = 0;
    start = 1;
    while( *tls != 42 );
    assert( 0 ); /* ERROR */
}
