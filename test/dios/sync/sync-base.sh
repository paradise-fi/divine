# TAGS: min
. lib/testcase

tee simple.cpp <<EOF
#include <dios.h>
#include <sys/divm.h>

volatile int glob = 0;

void routine1()
{
    glob = 1;
    __dios_trace_f( "A1: %d", glob );
    __dios_yield();
    __dios_trace_f( "A2: %d", glob );
}

void routine2()
{
    glob = 2;
    __dios_trace_f( "B1: %d", glob );
    __dios_yield();
    __dios_trace_f( "B2: %d", glob );
}

int main()
{
    __dios_sync_task( routine1 );
    __dios_sync_task( routine2 );
    __dios_trace_t( "Setup done" );
}
EOF

sim --synchronous simple.cpp <<EOF
> start
> stepa
+ Setup done
> stepa
+ T: (.*) A1: 1
+ T: (.*) B1: 2
> stepa
+ T: (.*) A2: 2
+ T: (.*) B2: 2
> stepa
+ T: (.*) A1: 1
+ T: (.*) B1: 2
EOF
