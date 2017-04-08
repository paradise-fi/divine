#include <sys/monitor.h>
#include <sys/syscall.h>

void __dios::register_monitor( __dios::Monitor *monitor ) noexcept
{
    __dios_syscall( SYS_register_monitor, nullptr, monitor );
}
