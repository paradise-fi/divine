#include <sys/monitor.h>
#include <sys/divm.h>
#include <sys/syscall.h>

void __dios::register_monitor( __dios::Monitor *monitor ) noexcept
{
    __dios_register_monitor( monitor );
}

void monitor_accept()
{
    __vm_control(_VM_CA_Bit, _VM_CR_Flags, _VM_CF_Accepting, _VM_CF_Accepting);
}

int monitor_choose( int range )
{
    return __vm_choose( range );
}

void monitor_cancel()
{
    __vm_control(_VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel);
}
