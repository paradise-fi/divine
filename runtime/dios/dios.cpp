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

namespace __dios {
using VFS = divine::fs::VFS;

Context::Context() :
    scheduler( __dios::new_object< Scheduler >() ),
    syscall( __dios::new_object< Syscall >() ),
    fault( __dios::new_object< Fault >() ),
    vfs( __dios::new_object< VFS >() ),
    globals( __vm_control( _VM_CA_Get, _VM_CR_Globals ) )
{}

void MachineParams::initialize( const SysOpts& opts ) {
    hardwareConcurrency = 0;
    for( const auto& i : opts ) {
        if ( i.first == "machine-concurrency" ) {
            char *end;
            hardwareConcurrency = strtol( i.second.c_str(), &end, 10 );
            if ( i.second.data() == end || end - 1 != &i.second.back() )
                __dios_fault( _DiOS_F_Config,
                    "DiOS boot configuration: invalid machine-concurrency specified" );
        }
    }
}

void MachineParams::traceParams( int indent ) {
    __dios_trace_i( indent, "machine parameters:" );
    __dios_trace_i( indent + 1, "- hardware_concurrency: %d", hardwareConcurrency );
}

bool trace_threads( const SysOpts& o ) {
    for( const auto& opt : o ) {
        if ( opt.first == "notrace" || opt.first == "trace" ) {
            bool trace = opt.first == "trace";

            if ( opt.second == "threads" || opt.second == "thread" )
                return trace;
            else {
                __dios_trace_f( "Warning: uknown tracing param \"%s\"", opt.second.c_str() );
                return false;
            }
        }
    }
    return false;
}

void trace_help() {
    __dios_trace_i( 0, "help:" );
    __dios_trace_i( 1,   "Supported commands:" );
    __dios_trace_i( 1,   "- {trace, notrace}: report/not report item back to VM" );
    __dios_trace_i( 2,     "- threads" );
    __dios_trace_i( 1,   "- [force-]{ignore, report, abort}: configure fault, "
                            "force disables program override" );
    __dios_trace_i( 2,     "- assert" );
    __dios_trace_i( 2,     "- arithtmetic" );
    __dios_trace_i( 2,     "- memory" );
    __dios_trace_i( 2,     "- control" );
    __dios_trace_i( 2,     "- locking" );
    __dios_trace_i( 2,     "- hypercall" );
    __dios_trace_i( 2,     "- notimplemented" );
    __dios_trace_i( 2,     "- diosassert" );
    __dios_trace_i( 1,   "- {nofail, simfail}: enable/disable simulation of failure" );
    __dios_trace_i( 2,     "- malloc" );
    __dios_trace_i( 1,   "- hardware_concurrency:{num} specify number of harware concurrency units (defulat 0)");
}

void trace_env( const _VM_Env *env ) {
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
    trace_help();
    trace_env( env );

    // Activate temporary scheduler to handle errors
    __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<false> );

    SysOpts sys_opts;
    if ( !get_sys_opt( env, sys_opts ) ) {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        return;
    }

    // Select scheduling mode
    if ( trace_threads( sys_opts ) )
        __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<true> );
    else
        __vm_control( _VM_CA_Set, _VM_CR_Scheduler, __dios::sched<false> );

    auto context = new_object< Context >();
    __vm_trace( _VM_T_StateType, context );
    __vm_control( _VM_CA_Set, _VM_CR_State, context );

    if ( !context->fault->load_user_pref( sys_opts ) ) {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        return;
    }
    context->fault->trace_config( 0 );

    context->machineParams.initialize( sys_opts );
    context->machineParams.traceParams( 0 );

    auto argv = construct_main_arg( "arg.", env, true );
    auto envp = construct_main_arg( "env.", env );
    trace_main_arg(0, "main argv", argv );
    trace_main_arg(0, "main envp", envp );
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
