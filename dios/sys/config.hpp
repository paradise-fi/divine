#pragma once

#include <dios/sys/scheduling.hpp>
#include <dios/sys/sync_scheduling.hpp>
#include <dios/sys/fair_scheduling.hpp>
#include <dios/sys/syscall.hpp>
#include <dios/sys/fault.hpp>
#include <dios/sys/monitor.hpp>
#include <dios/sys/machineparams.hpp>
#include <dios/sys/procmanager.hpp>

#include <dios/vfs/manager.h>

#include <dios/sys/upcall.hpp>

namespace __dios::config
{
    using Base = MachineParams< MonitorManager< BaseContext > >;
    template< typename B > using WithProc = fs::VFS< ProcessManager< Fault< B > > >;

    using Default = Upcall< WithProc< Scheduler< Base > > >;
//    using Passthrough = Upcall< Fault< Scheduler < fs::PassThrough < Base > > > >;
//    using Replay = Upcall< Fault< Scheduler < fs::Replay < Base > > > >;
    using Fair = Upcall< WithProc< FairScheduler< Base > > >;
    using Sync = Upcall< fs::VFS< Fault< SyncScheduler< Base > > > >;
}
