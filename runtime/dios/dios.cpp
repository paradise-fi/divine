// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

extern "C" {
#include <sys/utsname.h>
}

#include <dios.h>
#include <divine/opcodes.h>
#include <divine/metadata.h>
#include <dios/main.hpp>
#include <dios/scheduling.hpp>
#include <dios/syscall.hpp>
#include <dios/trace.hpp>
#include <dios/fault.hpp>
#include <filesystem/fs-manager.h>
#include <filesystem/fs-constants.h>

extern "C" {
#include <unistd.h>
char **environ;
}

namespace __dios {
using VFS = divine::fs::VFS;
using FileTrace = divine::fs::FileTrace;

Context::Context() :
    scheduler( __dios::new_object< Scheduler >() ),
    syscall( __dios::new_object< Syscall >() ),
    fault( __dios::new_object< Fault >() ),
    vfs( __dios::new_object< VFS >() ),
    globals( __vm_control( _VM_CA_Get, _VM_CR_Globals ) )
{}

void Context::finalize() {
    __dios::delete_object( vfs );
    __dios::delete_object( fault );
    __dios::delete_object( syscall );
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
    __dios_trace_i( indent + 1, "- ncpus: %d", hardwareConcurrency );
}

FileTrace getFileTraceConfig( const SysOpts& o, dstring stream ) {
    for ( const auto& opt : o ) {
        if ( stream == opt.first ) {
            dstring s;
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
};

TraceDebugConfig getTraceDebugConfig( const SysOpts& o ) {
    TraceDebugConfig config;
    for( const auto& opt : o ) {
        dstring key;
        std::transform( opt.first.begin(), opt.first.end(),
            std::back_inserter( key ), ::tolower );

        if ( key == "notrace" || key == "trace" ) {
            bool trace = opt.first == "trace";
            dstring what;
            std::transform( opt.second.begin(), opt.second.end(),
                std::back_inserter( what ), ::tolower );

            if ( what == "threads" || what == "thread" )
                config.threads = trace;
            else if ( what == "help" )
                config.help = trace;
            else if ( what == "rawenv" )
                config.raw = trace;
            else if ( what == "machineparams" )
                config.machineParams = trace;
            else if ( what == "mainargs" || what == "mainarg" )
                config.mainArgs = trace;
            else if ( what == "faultcfg" )
                config.faultCfg = trace;
            else
                __dios_trace_f( "Warning: uknown tracing param \"%s\"", opt.second.c_str() );
        }
        else if ( key == "debug" ) {
            dstring what;
            std::transform( opt.second.begin(), opt.second.end(),
                std::back_inserter( what ), ::tolower );

            if ( what == "help" )
                config.help = true;
            else if ( what == "rawenv" )
                config.raw = true;
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

void traceHelp() {
    __dios_trace_i( 0, "help:" );
    __dios_trace_i( 1,   "Supported commands:" );
    __dios_trace_i( 1,   "- debug: print debug information during boot" );
    __dios_trace_i( 2,     "- help: show help and exit" );
    __dios_trace_i( 2,     "- rawEnv: user DiOS boot parameters" );
    __dios_trace_i( 2,     "- machineParams: specified by user, e.g. number of cpus" );
    __dios_trace_i( 2,     "- mainArgs" );
    __dios_trace_i( 2,     "- faultCfg: fault and simfail configuration" );
    __dios_trace_i( 1,   "- {trace|notrace}: report/not report item back to VM" );
    __dios_trace_i( 2,     "- threads: trace thread info during execution" );
    __dios_trace_i( 1,   "- [force-]{ignore|report|abort}: configure fault, "
                            "force disables program override" );
    __dios_trace_i( 2,     "- assert" );
    __dios_trace_i( 2,     "- arithtmetic" );
    __dios_trace_i( 2,     "- memory" );
    __dios_trace_i( 2,     "- control" );
    __dios_trace_i( 2,     "- locking" );
    __dios_trace_i( 2,     "- hypercall" );
    __dios_trace_i( 2,     "- notimplemented" );
    __dios_trace_i( 2,     "- diosassert" );
    __dios_trace_i( 1,   "- {nofail|simfail}: enable/disable simulation of failure" );
    __dios_trace_i( 2,     "- malloc" );
    __dios_trace_i( 1,   "- ncpus:" );
    __dios_trace_i( 2,     "- <num> specify number of cpu units (default 0)" );
    __dios_trace_i( 1,   "- {stdout|stderr}: specify how to treat program output" );
    __dios_trace_i( 2,     "- notrace: ignore the stream" );
    __dios_trace_i( 2,     "- unbuffered: trace each write" );
    __dios_trace_i( 2,     "- trace: trace after each newline (default) " );
}

void traceEnv( const _VM_Env *env ) {
    __dios_trace_i (0, "raw env options:" );
     for ( ; env->key; env++ )
        __dios_trace_i( 1, "- %s: %.*s", env->key, env->size, env->value );
}

void init( const _VM_Env *env )
{
    // No active thread
    __vm_control( _VM_CA_Set, _VM_CR_User1, -1 );
    __vm_control( _VM_CA_Set, _VM_CR_FaultHandler, __dios::Fault::handler );

    __dios_trace_i( 0, "# DiOS boot info" );
    __dios_trace_i( 0, "---" );

    // Activate temporary scheduler to handle errors
    __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<false> );
    // Initialize context
    auto context = new_object< Context >();
    __vm_trace( _VM_T_StateType, context );
    __vm_control( _VM_CA_Set, _VM_CR_State, context );

    SysOpts sysOpts;
    if ( !getSysOpts( env, sysOpts ) ) {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        return;
    }
    TraceDebugConfig traceCfg = getTraceDebugConfig( sysOpts );

    if ( traceCfg.help ) {
        traceHelp();
        __dios_trace_i( 0, "---" );
        return;
    }

    if ( traceCfg.raw )
        traceEnv( env );

    // Select scheduling mode
    if ( traceCfg.threads )
        __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<true> );
    else
        __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<false> );
    
     context->vfs->instance( ).setOutputFile(getFileTraceConfig(sysOpts, "stdout" ));
     context->vfs->instance( ).setErrFile(getFileTraceConfig(sysOpts, "stderr" ));

    if ( !context->fault->load_user_pref( sysOpts ) ) {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        return;
    }

    if ( traceCfg.faultCfg )
        context->fault->trace_config( 0 );

    context->machineParams.initialize( sysOpts );
    if ( traceCfg.machineParams )
        context->machineParams.traceParams( 0 );

    auto argv = construct_main_arg( "arg.", env, true );
    auto envp = construct_main_arg( "env.", env );

    if ( traceCfg.mainArgs ) {
        trace_main_arg(0, "main argv", argv );
        trace_main_arg(0, "main envp", envp );
    }

    environ = envp.second;

    context->scheduler->startMainThread( argv.first, argv.second, envp.second );

    __dios_trace_i( 0, "---" );
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

/*
 * DiOS entry point. Defined weak to allow user to redefine it.
 */
extern "C" void  __attribute__((weak)) __boot( const _VM_Env *env ) {
    __dios::init( env );
}

int uname( struct utsname *__name ) {
    int ret;
    __dios_syscall( __dios::_SC_uname, &ret, __name );
    return ret;
}

int __dios_hardware_concurrency() noexcept {
    int ret;
    __dios_syscall( __dios::_SC_hardware_concurrency, &ret );
    return ret;
}

_Noreturn void __dios_jump( _VM_Frame *to, void (*pc)( void ), int restoreMaskTo ) noexcept
{
    bool m = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) ) & _VM_CF_Mask;
    if ( restoreMaskTo != -1 )
        m = restoreMaskTo;

    if ( m )
        __vm_control( _VM_CA_Set, _VM_CR_Frame, to,
                      _VM_CA_Set, _VM_CR_PC, pc );
    else
        __vm_control( _VM_CA_Set, _VM_CR_Frame, to,
                      _VM_CA_Set, _VM_CR_PC, pc,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, 0ull );
    __builtin_unreachable();
}

void __dios_unwind( _VM_Frame *stack, _VM_Frame *from, _VM_Frame *to ) noexcept
{
    bool m = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) ) & _VM_CF_Mask;
    if ( !stack ) {
        stack = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_Frame ) );
        // we must leave our caller intact so we can return to it
        if ( !from )
            from = stack->parent->parent;
    } else if ( !from )
        from = stack;

    _VM_Frame *fakeParentUpdateLocation;
    _VM_Frame **parentUpdateLocation = &fakeParentUpdateLocation;
    if ( stack != from ) {
        auto *f = stack;
        for ( ; f && f->parent != from; f = f->parent ) { }
        __dios_assert_v( f, "__dios_unwind reached end of the stack and did not find 'from' frame" );
        parentUpdateLocation = &f->parent;
    }

    // clean the frames, drop their allocas, jump
    // note: it is not necessary to clean the frames, it is only to prevent
    // accidental use of their variables, therefore it is also OK to not clean
    // current frame (heap will garbage-colect it)
    for ( auto *f = from; f != to; ) {
        auto *meta = __md_get_pc_meta( uintptr_t( f->pc ) );
        auto *inst = meta->inst_table;
        for ( int i = 0; i < meta->inst_table_size; ++i, ++inst )
        {
            if ( inst->opcode == OpCode::Alloca )
            {
                uintptr_t pc = uintptr_t( meta->entry_point ) + i;
                void *regaddr = __md_get_register_info( f, pc, meta ).start;
                void *regval = *static_cast< void ** >( regaddr );
                __vm_obj_free( regval );
            }
        }
        auto *old = f;
        f = f->parent;
        *parentUpdateLocation = f;
        __dios_assert_v( f || !to, "__dios_unwind reached end of the stack and did not find the target frame" );
        __vm_obj_free( old );
    }

    if ( !m )
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, 0ull );
}
