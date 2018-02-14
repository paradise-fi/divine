#include <sys/syscall.h>
#include <sys/fault.h>
#include <sys/trace.h>
#include <sys/divm.h>

int __dios_configure_fault( int fault, int cfg )
{
    int ret;
    __dios_syscall( SYS_configure_fault, &ret, fault, cfg );
    return ret;
}

int __dios_get_fault_config( int fault )
{
    int ret;
    __dios_syscall( SYS_get_fault_config, &ret, fault );
    return ret;
}

void __dios_fault( int f, const char *msg )
{
    if( msg )
        __dios_trace_f( "FAULT: %s", msg );
    __vm_fault_t fh = ( __vm_fault_t ) __vm_control( _VM_CA_Get, _VM_CR_FaultHandler );
    struct _VM_Frame *frame = __vm_control( _VM_CA_Get, _VM_CR_Frame );
    ( *fh )( f, frame->parent, frame->parent->pc );
}
