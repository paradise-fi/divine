/* TAGS: min c */
#include <sys/divm.h>

void fire()
{
    if ( __vm_choose( 2 ) )
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
}

int main()
{
    while ( 1 )
    {
        __dios_interrupt();
        __vm_trace( _VM_T_Text, "in cycle" );
        fire(); /* ERROR */
    }
}
