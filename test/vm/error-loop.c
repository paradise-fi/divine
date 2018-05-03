/* TAGS: min c */
#include <sys/divm.h>

void fire()
{
    if ( __vm_choose( 2 ) )
        __vm_ctl_flag( 0, _VM_CF_Error );
}

int main()
{
    while ( 1 )
    {
        __dios_suspend();
        __vm_trace( _VM_T_Text, "in cycle" );
        fire(); /* ERROR */
    }
}
