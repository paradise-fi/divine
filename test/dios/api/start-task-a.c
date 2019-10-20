/* TAGS: min c */
#include <dios.h>
#include <stdlib.h>
#include <assert.h>

volatile int start;

void *getTls( __dios_task id )
{
    return &id->__data;
}

void routine( void * x )
{
    while( !start );
    int *tls = getTls( __dios_this_task() );
    *tls = 42;
    while ( 1 );
}


int main()
{
    int *tls = getTls( __dios_start_task( routine, NULL, 2 * sizeof( int ) ) );
    *tls = 0;
    start = 1;
    while( *tls != 42 );
    assert( 0 ); /* ERROR */
}
