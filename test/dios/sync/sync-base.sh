# TAGS: min
. lib/testcase

tee simple.cpp <<EOF
#include <dios.h>
#include <sys/divm.h>

volatile int glob = 0;

void routine( void * x ){
    glob = 1;
    __dios_trace_f( "A1: %d", glob );
    __dios_yield();
    __dios_trace_f( "A2: %d", glob );
}

void routine2( void * x ){
    glob = 2;
    __dios_trace_f( "B1: %d", glob );
    __dios_yield();
    __dios_trace_f( "B2: %d", glob );
}

int main() {
    __dios_start_task( routine, nullptr, 0 );
    __dios_start_task( routine2, nullptr, 0 );
    __dios_trace_t( "Setup done" );
}
EOF

sim -oconfig:synchronous simple.cpp <<EOF
> start
> stepa
+ Setup done
> stepa
+ T: (.*) A1: 1
+ T: (.*) B1: 2
> stepa
+ T: (.*) A2: 2
+ T: (.*) A1: 1
+ T: (.*) B2: 1
+ T: (.*) B1: 2
> stepa
+ T: (.*) A2: 2
+ T: (.*) A1: 1
+ T: (.*) B2: 1
+ T: (.*) B1: 2
EOF
