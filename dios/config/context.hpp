#include <dios/sys/scheduling.hpp>
#include <dios/sys/sync_scheduling.hpp>
#include <dios/sys/fair_scheduling.hpp>
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
