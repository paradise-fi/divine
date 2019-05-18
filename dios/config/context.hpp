#include <dios/sys/sched_base.hpp>
#include <dios/sys/sched_sync.hpp>
#include <dios/sys/sched_fair.hpp>
#include <dios/sys/sched_null.hpp>
#include <dios/sys/syscall.hpp>
#include <dios/sys/fault.hpp>
#include <dios/sys/monitor.hpp>
#include <dios/sys/clock.hpp>
#include <dios/sys/machineparams.hpp>
#include <dios/sys/procmanager.hpp>
#include <dios/sys/upcall.hpp>
#include <dios/sys/boot.hpp>

#include <dios/vfs/manager.h>

namespace __dios
{
    struct Base : Clock< MachineParams< MonitorManager< BaseContext > > > {};
    template< typename B > using WithFS = fs::VFS< Fault< B > >;
    template< typename B > using WithProc = fs::VFS< ProcessManager< Fault< B > > >;
}
