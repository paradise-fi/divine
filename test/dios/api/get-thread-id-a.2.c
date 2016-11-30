#include <dios.h>
#include <assert.h>

volatile int start;
volatile _DiOS_ThreadHandle foreignId;

void routine( void * x ){
    while( !start );
    assert( __dios_get_thread_handle() == foreignId );
}

int main() {
    _DiOS_ThreadHandle myId1 = __dios_get_thread_handle();
    foreignId = __dios_start_thread( routine, NULL, 0 );
    _DiOS_ThreadHandle myId2 = __dios_get_thread_handle();
    start = 1;
    assert( myId1 == myId2 );
    assert( myId1 != foreignId );
}
