#include <dios.h>
#include <assert.h>

volatile int start;
volatile _DiOS_TaskHandle foreignId;

void routine( void * x ){
    while( !start );
    assert( __dios_get_task_handle() == foreignId );
}

int main() {
    _DiOS_TaskHandle myId1 = __dios_get_task_handle();
    foreignId = __dios_start_task( routine, NULL, 0 );
    _DiOS_TaskHandle myId2 = __dios_get_task_handle();
    start = 1;
    assert( myId1 == myId2 );
    assert( myId1 != foreignId );
}
