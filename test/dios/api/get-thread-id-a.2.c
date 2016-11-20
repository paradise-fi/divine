#include <dios.h>
#include <assert.h>

volatile int start;
volatile _DiOS_ThreadId foreignId;

void routine( void * x ){
    while( !start );
    assert( __dios_get_thread_id() == foreignId );
}

int main() {
    _DiOS_ThreadId myId1 = __dios_get_thread_id();
    foreignId = __dios_start_thread( routine, NULL, _DiOS_TLS_Reserved );
    _DiOS_ThreadId myId2 = __dios_get_thread_id();
    start = 1;
    assert( myId1 == myId2 );
    assert( myId1 != foreignId );
}
