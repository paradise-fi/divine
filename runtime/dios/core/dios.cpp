// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

extern "C" {
#include <sys/utsname.h>
}

#include <dios.h>
#include <divine/opcodes.h>
#include <divine/metadata.h>
#include <dios/core/main.hpp>
#include <dios/core/scheduling.hpp>
#include <dios/core/syscall.hpp>
#include <dios/core/trace.hpp>
#include <dios/core/fault.hpp>
#include <dios/filesystem/fs-manager.h>
#include <dios/filesystem/fs-constants.h>

extern "C" {
#include <unistd.h>
char **environ;
}

namespace __dios {
using FileTrace = fs::FileTrace;

Context::Context() :
    scheduler( __dios::new_object< Scheduler >() ),
    fault( __dios::new_object< Fault >() ),
    vfs( __dios::new_object< VFS >() ),
    globals( __vm_control( _VM_CA_Get, _VM_CR_Globals ) ),
    monitors( nullptr ),
    sighandlers( nullptr )
{}

void Context::finalize() {
    __dios::delete_object( vfs );
    __dios::delete_object( fault );
    __dios::delete_object( scheduler );
}

void MachineParams::initialize( const SysOpts& opts ) {
    hardwareConcurrency = 0;
    for( const auto& i : opts ) {
        if ( i.first == "ncpus" ) {
            char *end;
            hardwareConcurrency = strtol( i.second.c_str(), &end, 10 );
            if ( i.second.data() == end || end - 1 != &i.second.back() )
                __dios_fault( _DiOS_F_Config,
                    "DiOS boot configuration: invalid ncpus specified" );
        }
    }
}

void MachineParams::traceParams( int indent ) {
    __dios_trace_i( indent, "machine parameters:" );
    __dios_trace_i( indent + 1, "ncpus: %d", hardwareConcurrency );
}

FileTrace getFileTraceConfig( const SysOpts& o, String stream ) {
    for ( const auto& opt : o ) {
        if ( stream == opt.first ) {
            String s;
            std::transform( opt.second.begin(), opt.second.end(),
                std::back_inserter( s ), ::tolower );
            if ( s == "notrace" )
                return FileTrace::NOTRACE;
            if ( s == "unbuffered" )
                return FileTrace::UNBUFFERED;
            if ( s == "trace" )
                return FileTrace::TRACE;
        }
    }
    return FileTrace::TRACE;
}

bool useSyscallPassthrough( const SysOpts& o ) {
    for ( const auto& opt : o ) {
        if ( opt.first == "syscall" ) {
            if ( opt.second == "simulate" )
                return false;
            if ( opt.second == "passthrough" )
                return true;
            __dios_fault( _DiOS_F_Config,
                "DiOS boot configuration: invalid syscall option" );
        }
    }
    return false;
}

struct TraceDebugConfig {
    bool threads:1;
    bool help:1;
    bool raw:1;
    bool machineParams:1;
    bool mainArgs:1;
    bool faultCfg:1;

    TraceDebugConfig() :
        threads( false ), help( false ), raw( false ), machineParams( false ),
        mainArgs( false ), faultCfg( false ) {}

    bool anyDebugInfo() {
        return help || raw || machineParams || mainArgs || faultCfg;
    }
};

TraceDebugConfig getTraceDebugConfig( const SysOpts& o ) {
    TraceDebugConfig config;
    for( const auto& opt : o ) {
        String key;
        std::transform( opt.first.begin(), opt.first.end(),
            std::back_inserter( key ), ::tolower );

        if ( key == "notrace" || key == "trace" ) {
            bool trace = opt.first == "trace";
            String what;
            std::transform( opt.second.begin(), opt.second.end(),
                std::back_inserter( what ), ::tolower );

            if ( what == "threads" || what == "thread" )
                config.threads = trace;
            else
                __dios_trace_f( "Warning: uknown tracing param \"%s\"", opt.second.c_str() );
        }
        else if ( key == "debug" ) {
            String what;
            std::transform( opt.second.begin(), opt.second.end(),
                std::back_inserter( what ), ::tolower );

            if ( what == "help" )
                config.help = true;
            // ToDo: Trace binary blobs
            /*else if ( what == "rawenvironment" )
                config.raw = true;*/
            else if ( what == "machineparams" )
                config.machineParams = true;
            else if ( what == "mainargs" || what == "mainarg" )
                config.mainArgs = true;
            else if ( what == "faultcfg" )
                config.faultCfg = true;
            else
                __dios_trace_f( "Warning: uknown debug param \"%s\"", opt.second.c_str() );
        }
    }
    return config;
}



void traceHelpOption( int i, String opt, String desc, const Vector<String>& args ) {
    __dios_trace_i( i, "- \"%s\":", opt.c_str() );
    __dios_trace_i( i + 2, "description: %s", desc.c_str() );
    __dios_trace_i( i + 2, "arguments:" );
    for ( const auto& arg : args )
        __dios_trace_i( i + 3, "- %s", arg.c_str() );
}

void traceHelp( int i ) {
    __dios_trace_i( i, "help:" );
    __dios_trace_i( i + 1, "supported commands:" );
    traceHelpOption( i + 2, "debug", "print debug information during boot",
        { "help - help and exit",
          // ToDo: trace binary blobs
          /*"rawenvironment - user DiOS boot parameters",*/
          "machineparams - specified by user, e.g. number of cpus",
          "mainargs - argv and envp",
          "faultcfg - fault and simfail configuration" } );
    traceHelpOption( i + 2, "{trace|notrace}",
        "report/not report item back to VM",
        { "threads - thread info during execution"} );
    traceHelpOption( i + 2, "[force-]{ignore|report|abort}",
        "configure fault, force disables program override",
        { "assert",
          "arithmetic",
          "memory",
          "control",
          "locking",
          "hypercall",
          "notimplemented",
          "diosassert" } );
    traceHelpOption( i + 2, "{nofail|simfail}",
        "enable/disable simulation of failure",
        { "malloc" } );
    traceHelpOption( i + 2, "ncpus", "specify number of cpu units (default 0)", { "<num>" } );
    traceHelpOption( i + 2, "{stdout|stderr}", "specify how to treat program output",
        { "notrace - ignore the stream",
          "unbuffered - trace each write",
          "trace - trace after each newline (default)"} );
    traceHelpOption( i + 2, "syscall", "specify how to treat standard syscalls",
        { "simulate - simulate syscalls, use virtual file system (can be used with verify and run)",
          "passthrough - use syscalls from the underlying host OS (cannot be used with verify) "} );
}

void traceEnv( int ind, const _VM_Env *env ) {
    __dios_trace_i( ind, "raw env options:" );
    for ( ; env->key; env++ ) {
        __dios_trace_i( ind + 1, "%s: \"%.*s\"", env->key, env->size, env->value );
    }
}

void init( const _VM_Env *env )
{
    // No active thread
    __vm_control( _VM_CA_Set, _VM_CR_User1, nullptr );
    __vm_control( _VM_CA_Set, _VM_CR_FaultHandler, __dios::Fault::handler );

    // Activate temporary scheduler to handle errors
    __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<false> );

    // Initialize context
    auto context = new_object< Context >();
    __vm_trace( _VM_T_StateType, context );
    __vm_control( _VM_CA_Set, _VM_CR_State, context );

    auto mainthr = context->scheduler->newThread( _start, 0 );

    auto argv = construct_main_arg( "arg.", env, true );
    auto envp = construct_main_arg( "env.", env );

    context->scheduler->setupMainThread( mainthr, argv.first, argv.second, envp.second );
    SysOpts sysOpts;
    if ( !getSysOpts( env, sysOpts ) ) {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        return;
    }
    TraceDebugConfig traceCfg = getTraceDebugConfig( sysOpts );

    if ( traceCfg.anyDebugInfo() ) {
        __dios_trace_i( 0, "DiOS boot info:" );
    }

    if ( traceCfg.help ) {
        traceHelp( 1 );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel, _VM_CF_Cancel );
        return;
    }

    if ( traceCfg.raw )
        traceEnv( 1, env );

    if ( useSyscallPassthrough(sysOpts) ){
        _DiOS_SysCalls = _DiOS_SysCalls_Passthru;
    }

    // Select scheduling mode
    if ( traceCfg.threads )
        __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<true> );
    else
        __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<false> );

     context->vfs->instance( ).setOutputFile(getFileTraceConfig(sysOpts, "stdout" ));
     context->vfs->instance( ).setErrFile(getFileTraceConfig(sysOpts, "stderr" ));
     context->vfs->instance( ).initializeFromSnapshot( env );

    if ( !context->fault->load_user_pref( sysOpts ) ) {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        return;
    }

    if ( traceCfg.faultCfg )
        context->fault->trace_config( 1 );

    context->machineParams.initialize( sysOpts );
    if ( traceCfg.machineParams )
        context->machineParams.traceParams( 1 );

    if ( traceCfg.mainArgs ) {
        trace_main_arg( 1, "main argv", argv );
        trace_main_arg( 1, "main envp", envp );
    }

    environ = envp.second;

}

} // namespace __dios

namespace __sc {

void uname( __dios::Context&, int *, void *ret, va_list vl )
{
    utsname *name = va_arg( vl, utsname * );
    strcpy( name->sysname, "DiOS" );
    strcpy( name->nodename, "" );
    strcpy( name->release, "0.1" );
    strcpy( name->version, "0" );
    strcpy( name->machine, "DIVINE 4 VM" );
    *static_cast< int * >( ret ) = 0;
}

void hardware_concurrency (__dios::Context& c, int *, void *ret, va_list ) {
    *static_cast< int * >( ret ) = c.machineParams.hardwareConcurrency;
}

} // namespace __sc

namespace __sc_passthru {

void uname( __dios::Context& ctx, int * err, void* retval, va_list vl )  {
    __sc::uname(ctx, err, retval, vl);
}

void hardware_concurrency( __dios::Context& ctx, int * err, void* retval, va_list vl ) {
    __sc::hardware_concurrency(ctx, err, retval, vl);
}

} // namespace __sc_passthru

/*
 * DiOS entry point. Defined weak to allow user to redefine it.
 */
extern "C" void  __attribute__((weak)) __boot( const _VM_Env *env ) {
    __dios::init( env );
}

int uname( struct utsname *__name ) {
    int ret;
    __dios_syscall( SYS_uname, &ret, __name );
    return ret;
}

int __dios_hardware_concurrency() noexcept {
    int ret;
    __dios_syscall( SYS_hardware_concurrency, &ret );
    return ret;
}

