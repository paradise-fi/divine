. lib/testcase

sim $TESTS/c/1.malloc.c <<EOF
+ ^# executing __boot at
> trace 0 0 1
- FAULT
+ ^traced states: #1 #2
+ ^# executing malloc at
> stepa --count 3
+ ^# executing - at -
> trace 0 0 0
+ ^traced states: #1 #2
+ ^# executing malloc at
> step --count 5
+ ^T: FAULT: null pointer dereference
+ ^# executing __dios::Fault::handler
> trace 0 0 0 0
+ traced states: #1 #2 #6
+ unused choices: 0
EOF

tee loop.c <<EOF
#include <divine.h>

void __sched() {}
int main() {}

void __boot()
{
    __vm_control( _VM_CA_Set, _VM_CR_State, __vm_obj_make( 4 ) );
    __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __sched );
}
EOF

sim loop.c <<EOF
> trace 0 0
+ traced states: #1 #1 \[loop closed\]
+ unused choices: 0 0
EOF
