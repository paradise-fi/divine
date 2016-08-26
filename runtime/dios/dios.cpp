// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <dios.h>
#include <divine/opcodes.h>
#include <dios/main.h>
#include <dios/scheduling.h>
#include <dios/syscall.h>
#include <dios/trace.h>
#include <dios/fault.h>

namespace __dios {

Context::Context() :
    scheduler( __dios::new_object< Scheduler >() ),
    syscall( __dios::new_object< Syscall >() ),
    fault( __dios::new_object< Fault >() ) {}

void *init( const _VM_Env *env ) {
    const _VM_Env *e = env;
    while ( e->key ) {
        __dios_trace_f( "Key: %s, Value: %.*s", e->key, e->size, e->value );
        ++e;
    }

    __dios_trace_t( "__sys_init called" );
    __vm_set_fault( __dios::Fault::handler );

    // Select scheduling mode
    auto sched_arg = get_env_key( "divine.runmode", env );
    __dios_assert_v( sched_arg, "divine.runmode not provided" );
    __dios_assert_v( sched_arg->size == 1, "divine.runmode has unxpected size (uint8_t expected)" );
    auto mode = reinterpret_cast< const uint8_t * >( sched_arg->value )[0];
    if ( mode == _VM_R_Run || mode == _VM_R_Sim )
        __vm_set_sched( __dios::sched<true> );
    else
        __vm_set_sched( __dios::sched<false> );

    auto context = new_object< Context >();
    context->fault->load_user_pref( env );

    // Find & run main function
    _DiOS_FunPtr main = __dios_get_fun_ptr( "main" );
    if ( !main ) {
        __dios_trace_t( "No main function" );
        __vm_fault( static_cast< _VM_Fault >( _DiOS_F_MissingFunction ), "main" );
        return nullptr;
    }

    auto argv = construct_main_arg( "arg.", env, true );
    auto envp = construct_main_arg( "env.", env );
    context->scheduler->start_main_thread( main, argv.first, argv.second, envp.second );
    __dios_trace_t( "Main thread started" );

    return context;
}

} // namespace __dios

namespace __sc {

void  dummy( __dios::Context&, void *ret, va_list vl ) {
    __dios_trace_t( "Dummy syscall issued!" );
}

} // namespace __sc

/*
 * DiOS entry point. Defined weak to allow user to redefine it.
 */
extern "C" void *  __attribute__((weak)) __sys_init( const _VM_Env *env ) {
    return __dios::init( env );
}

_DiOS_FunPtr __dios_get_fun_ptr( const char *name ) noexcept {
    return __md_get_function_meta( name );
}

void __dios_dummy() noexcept {
    __dios_syscall( __dios::_SC_DUMMY, nullptr );
}


_Noreturn void __dios_unwind( _VM_Frame *to, void (*pc)( void ) ) noexcept {
    auto m = __vm_mask( 1 );
    // clean the frames, drop their allocas, jump
    // note: it is not necessary to clean the frames, it is only to prevent
    // accidental use of their variables, therefore it is also OK to not clean
    // current frame (heap will garbage-colect it)
    auto *top = __vm_query_frame();
    for ( auto *f = top->parent; f != to; top->parent = f ) {
        auto *meta = __md_get_pc_meta( uintptr_t( f->pc ) );
        auto *inst = meta->inst_table;
        for ( int i = 0; i < meta->inst_table_size; ++i, ++inst ) {
            if ( inst->opcode == OpCode::Alloca )
                __vm_free_object( *static_cast< void ** >( __md_get_register_info( f,
                                        uintptr_t( meta->entry_point ) + i, meta ).start ) );
        }
        auto *old = f;
        f = f->parent;
        __vm_free_object( old );
        __dios_assert_v( f, "__dios_unwind reached end of the stack and did not found target frame" );
    }
    __vm_jump( to, pc, !m );
    __builtin_unreachable();
}
