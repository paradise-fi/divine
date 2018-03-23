# TAGS: min
. lib/testcase

tee loop.c <<EOF
#include <sys/divm.h>

void __sched() { __vm_suspend(); }
int main() {}

void __boot()
{
    __vm_ctl_set( _VM_CR_State, __vm_obj_make( 4 ) );
    __vm_ctl_set( _VM_CR_Scheduler, __sched );
    __vm_suspend();
}
EOF

sim loop.c <<EOF
> trace --choices 0 0
+ traced states: #1 #1 \[loop closed\]
+ unused choices: 0/0 0/0
EOF
