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
#include <dios/vfs/passthru.h>
#include <dios/vfs/replay.h>

namespace __dios::config
{
    using Base = MachineParams< MonitorManager< BaseContext > >;
    template< typename B > using WithProc = fs::VFS< ProcessManager< Fault< B > > >;

    using Default = WithProc< Scheduler< Base > >;
    using Passthrough = Fault< Scheduler < fs::PassThrough < Base > > >;
    using Replay = Fault< Scheduler < fs::Replay < Base > > >;
    using Fair = WithProc< FairScheduler< Base > >;
    using Sync = fs::VFS< Fault< SyncScheduler< Base > > >;
}
