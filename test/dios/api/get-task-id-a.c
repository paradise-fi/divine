/* TAGS: min c */
#include <dios.h>
#include <assert.h>

volatile int start;
volatile __dios_task foreignId;

void routine( void * x )
{
    while( !start );
    assert( __dios_this_task() == foreignId );
    __dios_suicide();
}

int main() {
    __dios_task myId1 = __dios_this_task();
    foreignId = __dios_start_task( routine, NULL, 0 );
    __dios_task myId2 = __dios_this_task();
    start = 1;
    assert( myId1 == myId2 );
    assert( myId1 != foreignId );
}
