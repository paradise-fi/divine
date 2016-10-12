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

namespace __dios {

Context::Context() :
    scheduler( __dios::new_object< Scheduler >() ),
    syscall( __dios::new_object< Syscall >() ),
    fault( __dios::new_object< Fault >() ),
    globals( __vm_control( _VM_CA_Get, _VM_CR_Globals ) )
{}

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

void init( const _VM_Env *env )
{
    // No active thread
    __vm_control( _VM_CA_Set, _VM_CR_User1, -1 );
    __vm_control( _VM_CA_Set, _VM_CR_FaultHandler, __dios::Fault::handler );

    const _VM_Env *e = env;
    while ( e->key ) {
        __dios_trace_f( "env: %s = %.*s", e->key, e->size, e->value );
        ++e;
    }

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

    auto argv = construct_main_arg( "arg.", env, true );
    auto envp = construct_main_arg( "env.", env );
    context->scheduler->startMainThread( argv.first, argv.second, envp.second );
}

} // namespace __dios

namespace __sc {

void uname( __dios::Context&, void *ret, va_list vl )
{
    utsname *name = va_arg( vl, utsname * );
    strcpy( name->sysname, "DiOS" );
    strcpy( name->nodename, "" );
    strcpy( name->release, "0.1" );
    strcpy( name->version, "0" );
    strcpy( name->machine, "DIVINE 4 VM" );
    *static_cast< int * >( ret ) = 0;
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
    __dios_syscall( __dios::_SC_UNAME, &ret, __name );
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
