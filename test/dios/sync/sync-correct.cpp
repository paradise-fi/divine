/* VERIFY_OPTS: --synchronous */
/* TAGS: c++ */
#include <dios.h>
#include <sys/divm.h>
#include <cassert>

volatile int glob = 0;

void routine1()
{
    glob = 1;
    __dios_trace_f( "A1: %d", glob );
    __dios_yield();
    assert( glob == 2 ); // async scheduler violates this
    glob = 1;
    __dios_trace_f( "A2: %d", glob );
}

void routine2()
{
    glob = 2;
    __dios_trace_f( "B1: %d", glob );
    __dios_yield();
    assert( glob == 1 ); // async scheduler violates this
    __dios_trace_f( "B2: %d", glob );
}

int main()
{
    __dios_sync_task( routine1 );
    __dios_sync_task( routine2 );
}
