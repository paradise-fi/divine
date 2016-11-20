#include <dios.h>
#include <assert.h>

volatile int value;

void routine( void * x ){
    while( 1 )
        value = 42;
}

int main() {
    value = 24;
    _DiOS_ThreadId thr = __dios_start_thread( routine, NULL, _DiOS_TLS_Reserved );
    while( value != 42 );
    __dios_kill_thread( thr );
    value = 24;
    assert( value == 24 );
    assert( 0 ); /* ERROR */
}
