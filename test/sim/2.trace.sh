. lib/testcase

sim $TESTS/c/1.malloc.c <<EOF
+ ^# executing __boot at
> trace --choices 0 0 1
- FAULT
+ ^traced states: #1 #2
+ ^# executing malloc at
> stepa --count 3
+ ^# executing - at -
> trace --choices 0 0 0
+ ^traced states: #1 #2
+ ^# executing malloc at
> step --out
> step --count 2
+ ^T: FAULT: null pointer dereference
+ ^# executing .*::handler
> trace --choices 0 0 0 0
+ traced states: #1 #2 #5
+ unused choices: 0
EOF

tee loop.c <<EOF
#include <sys/divm.h>

void __sched() {}
int main() {}

void __boot()
{
    __vm_control( _VM_CA_Set, _VM_CR_State, __vm_obj_make( 4 ) );
    __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __sched );
}
EOF

sim loop.c <<EOF
> trace --choices 0 0
+ traced states: #1 #1 \[loop closed\]
+ unused choices: 0 0
EOF
