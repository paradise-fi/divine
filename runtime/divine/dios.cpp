// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <divine/dios.h>
#include <divine/dios_syscall.h>
#include <divine/opcodes.h>

#include <tuple>
#include <utility>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <algorithm>

namespace dios {

using ThreadId = _DiOS_ThreadId;
using FunPtr   = _DiOS_FunPtr;

struct Syscall {
    Syscall() noexcept : _syscode( _SC_INACTIVE ) { }

    void call(int syscode, void* ret, va_list& args) noexcept {
        _syscode = static_cast< _DiOS_SC >( syscode );
        _ret = ret;
        va_copy( _args, args );
        __dios_syscall_trap();
        va_end( _args );
    }

    bool handle() noexcept {
        if ( _syscode != _SC_INACTIVE ) {
            ( *( _DiOS_SysCalls[ _syscode ] ) )( _ret, _args );
            _syscode = _SC_INACTIVE;
            return true;
        }
        return false;
    }

    static Syscall& get() noexcept {
        if ( !instance ) {
            instance = static_cast< Syscall * > ( __vm_make_object( sizeof( Syscall ) ) );
            new ( instance ) Syscall();
        }
        return *instance;
    }

private:
    _DiOS_SC _syscode;
    void *_ret;
    va_list _args;

    static Syscall *instance;
};

Syscall *Syscall::instance;

char *construct_argument( const _VM_Env *env ) {
    auto arg = static_cast< char * >( __vm_make_object( env->size + 1 ) );
    memcpy( arg, env->value, env->size );
    arg[ env->size ] = '\0';
    return arg;
}

// Constructs argument for main (argv, envp) from env and returns count and
// the constructed argument
std::pair<int, char**> construct_main_arg( const char* prefix, const _VM_Env *env,
    bool prepend_name = false )
{
    int argc = prepend_name ? 1 : 0;
    int pref_len = strlen( prefix );
    const _VM_Env *name = nullptr;
    const _VM_Env *e = env;
    for ( ; e->key; e++ ) {
        __dios_trace( 0, "Arg: %s", e->key );
        if ( memcmp( prefix, e->key, pref_len ) == 0 ) {
            argc++;
        }
        else if ( strcmp( e->key, "divine.bcname" ) == 0 ) {
            __dios_assert_v( !name, "Multiple divine.bcname provided" );
            name = e;
        }
    }
    auto argv = static_cast< char ** >( __vm_make_object( ( argc + 1 ) * sizeof( char * ) ) );

    char **arg = argv;
    if (prepend_name) {
        __dios_assert_v( name, "Missing binary name: divine.bcname" );
        *argv = construct_argument( name );
        arg++;
    }

    for ( ; env->key; env++ ) {
        if ( memcmp( prefix, env->key, pref_len ) == 0 ) {
            *arg = construct_argument( env );
            arg++;
        }
    }
    *arg = nullptr;

    return { argc, argv };
}

// Free resources allocated for arguments of the main function
void free_main_arg( char** argv ) {
    while( *argv ) {
        __vm_free_object( *argv );
        ++argv;
    }
    __vm_free_object( argv );
}

struct DiosMainFrame : _VM_Frame {
    int l;
    int argc;
    char** argv;
    char** envp;
};

struct ThreadRoutineFrame : _VM_Frame {
    void* arg;
};

struct CleanupFrame : _VM_Frame {
    int reason;
};

struct Thread {
    enum class State { RUNNING, CLEANING_UP, ZOMBIE };
    _VM_Frame *_frame;
    FunPtr _cleanup_handler;
    State _state;

    Thread( FunPtr fun, FunPtr cleanup = nullptr )
        : _frame( static_cast< _VM_Frame * >( __vm_make_object( fun->frame_size ) ) ),
          _cleanup_handler( cleanup ),
          _state( State::RUNNING )
    {
        _frame->pc = fun->entry_point;
        _frame->parent = nullptr;
        __dios_trace( 0, "Thread constuctor: %p, %p", _frame, _frame->pc );
    }

    Thread( const Thread& o ) = delete;
    Thread& operator=( const Thread& o ) = delete;

    Thread( Thread&& o ) :
        _frame( o._frame ), _cleanup_handler( o._cleanup_handler ),
        _state( o._state )
    {
        o._frame = 0;
        o._state = State::ZOMBIE;
    }

    Thread& operator=( Thread&& o ) {
        std::swap( _frame, o._frame );
        std::swap( _cleanup_handler, o._cleanup_handler );
        std::swap( _state, o._state );
        return *this;
    }

    ~Thread() {
        clear();
    }

    bool active() const { return _state == State::RUNNING; }
    bool cleaning_up() const { return _state == State::CLEANING_UP; }
    bool zombie() const { return _state == State::ZOMBIE; }

    void update_state() {
        if ( !_frame )
            _state = State::ZOMBIE;
    }

    void stop_thread( int reason ) {
        if ( !active() ) {
            __vm_fault( static_cast< _VM_Fault >( _DiOS_F_Threading ) );
            return;
        }

        clear();
        auto* frame = reinterpret_cast< CleanupFrame * >( _frame );
        frame->pc = _cleanup_handler->entry_point;
        frame->parent = nullptr;
        frame->reason = reason;
        _state = State::CLEANING_UP;
    }

private:
    void clear() {
        while ( _frame ) {
            _VM_Frame *f = _frame->parent;
            __vm_free_object( _frame );
            _frame = f;
        }
    }
};

struct ControlFlow {
    ThreadId active_thread;
    int      thread_count;
    Thread   main_thread;
};

struct Scheduler {
    Scheduler() {
        __dios_assert( _cf );
    }

    Scheduler( void *cf ) {
        _cf =  static_cast< ControlFlow * >( cf );
        __dios_assert( cf );
    }

    Thread* get_threads() const noexcept {
        return &( _cf->main_thread );
    }

    void *get_cf() const noexcept {
        return _cf;
    }

    _VM_Frame *run_thread( int idx = -1 ) noexcept {
        if ( idx < 0 )
            idx = _cf->active_thread;
        else
            _cf->active_thread = idx;

        Thread &thread = get_threads()[ idx ];
        thread.update_state();

        if ( !thread.zombie() ) {
            __vm_set_ifl( &( thread._frame) );
            return thread._frame;
        }

        __dios_trace( 0, "Thread exit" );
        // ToDo: Move main thread. Neccessary only for divine run
        if ( idx != 0 ) {
            return run_thread(0);
        }
        _cf = nullptr;
        return nullptr;
    }

    _VM_Frame *run_threads() noexcept {
        // __dios_trace( 0, "Number of threads: %d", _cf->thread_count );
        return run_thread( __vm_choose( _cf->thread_count ) );
    }

    void start_main_thread( FunPtr main, int argc, char** argv, char** envp ) noexcept {
        __dios_assert_v( main, "Invalid main function!" );

        _DiOS_FunPtr dios_main = __dios_get_fun_ptr( "__dios_main" );
        __dios_assert_v( dios_main, "Invalid DiOS main function" );

        new ( &( _cf->main_thread ) ) Thread( dios_main );
        _cf->active_thread = 0;
        _cf->thread_count = 1;

        DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >( _cf->main_thread._frame );
        frame->l = main->arg_count;

        frame->argc = argc;
        frame->argv = argv;
        frame->envp = envp;
    }

    ThreadId start_thread( FunPtr routine, void *arg, FunPtr cleanup ) {
        __dios_assert( routine );

        int cur_size = __vm_query_object_size( _cf );
        void *new_cf = __vm_make_object( cur_size + sizeof( Thread ) );
        memcpy( new_cf, _cf, cur_size );
        __vm_free_object( _cf );
        _cf = static_cast< ControlFlow * >( new_cf );

        Thread &t = get_threads()[ _cf->thread_count++ ];
        new ( &t ) Thread( routine, cleanup );
        ThreadRoutineFrame *frame = reinterpret_cast< ThreadRoutineFrame * >(
            t._frame );
        frame->arg = arg;

        return _cf->thread_count - 1;
    }

    void kill_thread( ThreadId t_id, int reason ) {
        __dios_assert( t_id );
        __dios_assert( int( t_id ) < _cf->thread_count );
        get_threads()[ t_id ].stop_thread( reason );
    }

    ThreadId get_thread_id() {
        return _cf->active_thread;
    }
private:
    static ControlFlow* _cf;
};

ControlFlow *Scheduler::_cf;

} // namespace dios

void *__dios_sched( int st_size, void *_state ) noexcept;
int main(...);

struct CtorDtorEntry {
    int32_t prio;
    void (*fn)();
    void *ignored; // should be used only by linker to discard entries
};

template< typename Sort >
static void runCtorsDtors( const char *name, Sort sort ) {
    auto *meta = __md_get_global_meta( name );
    if ( !meta )
        return;
    auto *begin = reinterpret_cast< CtorDtorEntry * >( meta->address ),
         *end = begin + meta->size / sizeof( CtorDtorEntry );
    std::sort( begin, end, sort );
    for ( ; begin != end; ++begin )
        begin->fn();
}

static void runCtors() {
    runCtorsDtors( "llvm.global_ctors",
            []( CtorDtorEntry &a, CtorDtorEntry &b ) { return a.prio < b.prio; } );
}

static void runDtors() {
    runCtorsDtors( "llvm.global_dtors",
            []( CtorDtorEntry &a, CtorDtorEntry &b ) { return a.prio > b.prio; } );
}

extern "C" void __dios_main( int l, int argc, char **argv, char **envp ) {
    __vm_mask( 1 );
    __dios_trace( 0, "Dios started!" );
    runCtors();
    int res;
    switch (l) {
    case 0:
        __vm_mask( 0 );
        res = main();
        break;
    case 2:
        __vm_mask( 0 );
        res = main( argc, argv );
        break;
    case 3:
        __vm_mask( 0 );
        res = main( argc, argv, envp );
        break;
    default:
        __dios_assert_v( false, "Unexpected prototype of main" );
        res = 256;
    }
    __vm_mask( 1 );

    if ( res != 0 )
        __vm_fault( ( _VM_Fault ) _DiOS_F_MainReturnValue );

    runDtors();

    dios::free_main_arg( argv );
    dios::free_main_arg( envp );

    __dios_trace( 0, "DiOS out!" );
}

void *__dios_sched( int, void *state ) noexcept {
    dios::Scheduler scheduler( state );
    if ( dios::Syscall::get().handle() ) {
        __vm_jump( scheduler.run_thread(), nullptr, 1 );
        return scheduler.get_cf();
    }

    _VM_Frame *jmp = scheduler.run_threads();
    if ( jmp ) {
        __vm_jump( jmp, nullptr, 1 );
    }

    return scheduler.get_cf();
}

void __dios_fault( enum _VM_Fault what, _VM_Frame *cont_frame, void (*cont_pc)() ) noexcept __attribute__((__noreturn__));

extern "C" void *__dios_init( const _VM_Env *env ) {
    const _VM_Env *e = env;
    while ( e->key ) {
        __dios_trace( 0, "Key: %s, Value: %.*s", e->key, e->size, e->value );
        ++e;
    }

    __vm_trace( "__sys_init called" );
    __vm_set_sched( __dios_sched );
    __vm_set_fault( __dios_fault );

    void *cf = __vm_make_object( sizeof( dios::ControlFlow ) );
    dios::Scheduler scheduler( cf );

    _DiOS_FunPtr main = __dios_get_fun_ptr( "main" );
    if ( !main ) {
        __vm_trace( "No main function" );
        __vm_fault( static_cast< _VM_Fault >( _DiOS_F_MissingFunction ), "main" );
        return nullptr;
    }

    /* ToDo: Parse and forward main arguments */
    auto argv = dios::construct_main_arg( "arg.", env, true );
    auto envp = dios::construct_main_arg( "env.", env );
    scheduler.start_main_thread( main, argv.first, argv.second, envp.second );
    __vm_trace( "Main thread started" );

    return scheduler.get_cf();
}

extern "C" void *__sys_init( const _VM_Env *env ) __attribute__((weak)) {
    return __dios_init( env );
}

_DiOS_FunPtr __dios_get_fun_ptr( const char *name ) noexcept {
    return __md_get_function_meta( name );
}

_DiOS_ThreadId __dios_start_thread( _DiOS_FunPtr routine, void *arg,
    _DiOS_FunPtr cleanup ) noexcept
{
    _DiOS_ThreadId ret;
    __dios_syscall( _SC_START_THREAD, &ret, routine, arg, cleanup );
    return ret;
}

_DiOS_ThreadId __dios_get_thread_id() noexcept {
    _DiOS_ThreadId ret;
    __dios_syscall( _SC_GET_THREAD_ID, &ret );
    return ret;
}

void __dios_kill_thread( _DiOS_ThreadId id, int reason ) noexcept {
    __dios_syscall( _SC_KILL_THREAD, nullptr, id, reason );
}

void __dios_dummy() noexcept {
    __dios_syscall( _SC_DUMMY, nullptr );
}

void __dios_syscall_trap() noexcept {
    int mask = __vm_mask( 1 );
    __vm_interrupt( 1 );
    __vm_mask( 0 );
    __vm_mask( 1 );
    __vm_mask( mask );
}

void __dios_syscall( int syscode, void* ret, ... ) {
    int mask = __vm_mask( 1 );

    va_list vl;
    va_start( vl, ret );
    dios::Syscall::get().call( syscode, ret, vl );
    va_end( vl );
    __vm_mask( mask );
}

namespace {
static bool inTrace = false;

struct InTrace {
    InTrace() : prev( inTrace ) {
        inTrace = true;
    }
    ~InTrace() { inTrace = prev; }

    bool prev;
};

void diosTraceInternalV( int indent, const char *fmt, va_list ap ) noexcept __attribute__((always_inline))
{
    static int fmtIndent = 0;
    InTrace _;

    char buffer[1024];
    for ( int i = 0; i < fmtIndent; ++i )
        buffer[ i ] = ' ';

    vsnprintf( buffer + fmtIndent, 1024, fmt, ap );
    __vm_trace( buffer );

    fmtIndent += indent * 4;
}

void diosTraceInternal( int indent, const char *fmt, ... ) noexcept
{
    va_list ap;
    va_start( ap, fmt );
    diosTraceInternalV( indent, fmt, ap );
    va_end( ap );
}
}

void __dios_trace( int indent, const char *fmt, ... ) noexcept
{
    int mask = __vm_mask(1);

    if ( inTrace )
        goto unmask;

    va_list ap;
    va_start( ap, fmt );
    diosTraceInternalV( indent, fmt, ap );
    va_end( ap );
unmask:
    __vm_mask(mask);
}

void __sc_start_thread( void *retval, va_list vl ) {
    auto routine = va_arg( vl, dios::FunPtr );
    auto arg = va_arg( vl, void * );
    auto cleanup = va_arg( vl, dios::FunPtr );
    auto ret = static_cast< dios::ThreadId * >( retval );

    *ret = dios::Scheduler().start_thread( routine, arg, cleanup );
}

void __sc_kill_thread( void *, va_list vl ) {
    auto id = va_arg( vl, dios::ThreadId );
    auto reason = va_arg( vl, int );

    dios::Scheduler().kill_thread( id, reason );
}

void __sc_get_thread_id( void *retval, va_list vl ) {
    auto ret = static_cast< dios::ThreadId * >( retval );
    *ret = dios::Scheduler().get_thread_id();
}

void __sc_dummy( void *ret, va_list vl ) {
    __vm_trace( "Dummy syscall issued!" );
}

void __dios_fault( enum _VM_Fault what, _VM_Frame *cont_frame, void (*cont_pc)() ) noexcept {
    auto mask = __vm_mask( 1 );
    InTrace _; // avoid dumping what we do

    /* ToDo: Handle errors */
    __vm_trace( "VM Fault" );
    switch ( what ) {
    case _VM_F_NoFault:
        diosTraceInternal( 0, "FAULT: No fault" );
        break;
    case _VM_F_Assert:
        diosTraceInternal( 0, "FAULT: Assert" );
        break;
    case _VM_F_Arithmetic:
        diosTraceInternal( 0, "FAULT: Arithmetic" );
        break;
    case _VM_F_Memory:
        diosTraceInternal( 0, "FAULT: Memory" );
        break;
    case _VM_F_Control:
        diosTraceInternal( 0, "FAULT: Control" );
        break;
    case _VM_F_Locking:
        diosTraceInternal( 0, "FAULT: Locking" );
        break;
    case _VM_F_Hypercall:
        diosTraceInternal( 0, "FAULT: Hypercall" );
        break;
    case _VM_F_NotImplemented:
        diosTraceInternal( 0, "FAULT: Not Implemented" );
        break;
    case _DiOS_F_MainReturnValue:
        diosTraceInternal( 0, "FAULT: Main exited with non-zero value" );
        break;
    default:
        diosTraceInternal( 0, "Unknown fault ");
    }
    diosTraceInternal( 0, "Backtrace:" );
    int i = 0;
    for ( auto *f = __vm_query_frame()->parent; f != nullptr; f = f->parent )
        diosTraceInternal( 0, "%d: %s", ++i, __md_get_pc_meta( uintptr_t( f->pc ) )->name );

    __vm_jump( cont_frame, cont_pc, !mask );
}

_Noreturn void __dios_unwind( _VM_Frame *to, void (*pc)( void ) ) noexcept {
    // clean the frames, drop their allocas, jump
    // note: it is not necessary to clean the frames, it is only to prevent
    // accidental use of their variables, therefore it is also OK to not clean
    // current frame (heap will garbage-colect it)
    for ( auto *f = __vm_query_frame()->parent; f != to; ) {
        auto *meta = __md_get_pc_meta( uintptr_t( f->pc ) );
        auto *inst = meta->inst_table;
        for ( int i = 0; i < meta->inst_table_size; ++i, ++inst ) {
            if ( inst->opcode == OpCode::Alloca )
                __vm_free_object( __md_get_register_info( f, uintptr_t( meta->entry_point ) + i, meta ).start );
        }
        auto *old = f;
        f = f->parent;
        __vm_free_object( old );
    }
    __vm_jump( to, pc, false );
}
