#include <sys/fault.h>

void __dios_fault( int f, const char *msg )
{
    if( msg )
        __vm_trace( _VM_T_Fault, msg );
    __vm_fault_t fh = ( __vm_fault_t ) __vm_ctl_get( _VM_CR_FaultHandler );
    ( *fh )( f, 0, 0 );
}
