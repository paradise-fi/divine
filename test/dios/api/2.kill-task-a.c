#include <dios.h>
#include <assert.h>

volatile int value;

void routine( void * x ){
    while( 1 )
        value = 42;
}

int main() {
    value = 24;
    _DiOS_TaskHandle thr = __dios_start_task( routine, NULL, 0 );
    while( value != 42 );
    __dios_kill_task( thr );
    value = 24;
    assert( value == 24 );
    assert( 0 ); /* ERROR */
}
