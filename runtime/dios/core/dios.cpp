// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <algorithm>
#include <unistd.h>

#include <sys/utsname.h>
#include <sys/bitcode.h>
#include <sys/metadata.h>

#include <dios.h>
#include <dios/core/main.hpp>
#include <dios/core/scheduling.hpp>
#include <dios/core/sync_scheduling.hpp>
#include <dios/core/fair_scheduling.hpp>
#include <dios/core/syscall.hpp>
#include <dios/core/trace.hpp>
#include <dios/core/fault.hpp>
#include <dios/core/monitor.hpp>
#include <dios/core/machineparams.hpp>
#include <dios/core/procmanager.hpp>
#include <dios/filesystem/fs-manager.h>
#include <dios/filesystem/fs-passthru.h>
#include <dios/filesystem/fs-replay.h>
#include <dios/filesystem/fs-constants.h>

extern "C" { char **environ; }

namespace __dios {

String extractDiosConfiguration( SysOpts& o ) {
    auto r = std::find_if( o.begin(), o.end(),
        []( const auto& opt ) {
            return opt.first == "config";
        } );
    String cfg( "default" );
    if ( r != o.end() ) {
        cfg = r->second;
        o.erase( r );
    }
    return cfg;
}

void traceHelpOption( int i, String opt, String desc, const Vector<String>& args ) {
    __dios_trace_i( i, "- %s: %s", opt.c_str(), desc.c_str() );
    __dios_trace_i( i, "  arguments:" );
    for ( const auto& arg : args )
        __dios_trace_i( i, "   - %s", arg.c_str() );
}

void traceHelp( int i, const Map< String, HelpOption >& help ) {
    for ( const auto& option : help )
        traceHelpOption( i, option.first, option.second.description,
                         option.second.options );
}

void traceEnv( int ind, const _VM_Env *env ) {
    __dios_trace_i( ind, "raw env options:" );
    for ( ; env->key; env++ ) {
        __dios_trace_i( ind + 1, "%s: \"%.*s\"", env->key, env->size, env->value );
    }
}

template < typename Configuration >
void boot( SetupBase sb ) {
    auto *context = new_object< Configuration >();
    __vm_trace( _VM_T_StateType, context );
    traceAlias< Configuration >( "{Context}" );
    __vm_control( _VM_CA_Set, _VM_CR_State, context );

    if ( extractOpt( "debug", "help", sb.opts ) )
    {
        Map< String, HelpOption > help = {
            { "config" , { "run DiOS in a given configuration",
                           { "default: async threads, processes, vfs",
                             "passthrough: pass syscalls to the host OS",
                             "replay: re-use a trace recorded in passthrough mode",
                             "synchronous: for use with synchronous systems" } } },
            { "debug", { "print debug information during boot",
                         { "help - help of selected configuration and exit",
                           // ToDo: trace binary blobs
                           /*"rawenvironment - user DiOS boot parameters",*/
                           "machineparams - specified by user, e.g. number of cpus",
                           "mainargs - argv and envp",
                           "faultcfg - fault and simfail configuration" } } }
        };
        context->getHelp( help );
        traceHelp( 0, help );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
        return;
    }

    if ( extractOpt( "debug", "rawenvironment", sb.opts ) )
    {
        traceEnv( 1, sb.env );
    }

    using Process = typename Configuration::Process;
    Setup< Configuration > s = sb;
    s.proc1 = new_object< Process >();
    context->setup( s );
}

void temporaryScheduler() {
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
}

void temporaryFaultHandler( _VM_Fault, _VM_Frame *, void (*)() )
{
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
    __vm_control( _VM_CA_Set, _VM_CR_Scheduler, nullptr );
    static_cast< _VM_Frame * >
            ( __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent = nullptr;
}

using DefaultConfiguration =
    fs::VFS< ProcessManager< Fault< Scheduler< MachineParams< MonitorManager< BaseContext > > > > > >;
using PassthruConfiguration =
    Fault< Scheduler < fs::PassThrough < MachineParams < MonitorManager < BaseContext > > > > >;

using ReplayConfiguration =
    Fault< Scheduler < fs::Replay < MachineParams < MonitorManager < BaseContext > > > > >;

using SynchronousConfiguration =
    fs::VFS< Fault< SyncScheduler< MachineParams< MonitorManager< BaseContext > > > > >;

using FairConfiguration =
    fs::VFS< ProcessManager< Fault< FairScheduler< MachineParams< MonitorManager< BaseContext > > > > > >;

void init( const _VM_Env *env )
{
    MemoryPool deterministicPool( 2 );

    __vm_control( _VM_CA_Set, _VM_CR_User1, nullptr );
    __vm_control( _VM_CA_Set, _VM_CR_FaultHandler, temporaryFaultHandler );
    __vm_control( _VM_CA_Set, _VM_CR_Scheduler, temporaryScheduler );


    SysOpts sysOpts;
    if ( !getSysOpts( env, sysOpts ) ) {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        return;
    }

    auto cfg = extractDiosConfiguration( sysOpts );
    SetupBase setup{ .pool = &deterministicPool, .env = env, .opts = sysOpts };
    if ( cfg == "default" )
        boot< DefaultConfiguration >( setup );
    else if ( cfg == "passthrough" ) {
        boot< PassthruConfiguration >( setup );
    } else if ( cfg == "replay" ) {
        boot< ReplayConfiguration >( setup );
    }
    else if ( cfg == "synchronous" ) {
        boot< SynchronousConfiguration >( setup );
    }
    else if ( cfg == "fair" ) {
        boot< FairConfiguration >( setup );
    }
    else {
        __dios_trace_f( "Unknown configaration: %s", cfg.c_str() );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
    }
}

} // namespace __dios

/*
 * DiOS entry point. Defined weak to allow user to redefine it.
 */
extern "C" void  __attribute__((weak)) __boot( const _VM_Env *env ) {
    __dios::init( env );
    __vm_suspend();
}
