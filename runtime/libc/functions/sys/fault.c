#include <sys/fault.h>
#include <sys/trace.h>
#include <sys/stack.h>
#include <sys/divm.h>

void __dios_fault( int f, const char *msg )
{
    if( msg )
        __dios_trace_f( "FAULT: %s", msg );
    __vm_fault_t fh = ( __vm_fault_t ) __vm_control( _VM_CA_Get, _VM_CR_FaultHandler );
    struct _VM_Frame *frame = __vm_control( _VM_CA_Get, _VM_CR_Frame );
    ( *fh )( f, frame->parent, frame->parent->pc );
}
