// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <dios/scheduling.hpp>

_DiOS_ThreadId __dios_start_thread( _DiOS_FunPtr routine, void *arg ) noexcept
{
    _DiOS_ThreadId ret;
    __dios_syscall( __dios::_SC_START_THREAD, &ret, routine, arg );
    return ret;
}

_DiOS_ThreadId __dios_get_thread_id() noexcept {
    _DiOS_ThreadId ret;
    __dios_syscall( __dios::_SC_GET_THREAD_ID, &ret );
    return ret;
}

void __dios_kill_thread( _DiOS_ThreadId id ) noexcept {
    __dios_syscall( __dios::_SC_KILL_THREAD, nullptr, id );
}

namespace __sc {

void start_thread( __dios::Context& ctx, void *retval, va_list vl ) {
    auto routine = va_arg( vl, __dios::FunPtr );
    auto arg = va_arg( vl, void * );
    auto ret = static_cast< __dios::ThreadId * >( retval );

    *ret = ctx.scheduler->start_thread( routine, arg );
}

void kill_thread( __dios::Context& ctx, void *, va_list vl ) {
    auto id = va_arg( vl, __dios::ThreadId );
    ctx.scheduler->kill_thread( id );
}

void get_thread_id( __dios::Context& ctx, void *retval, va_list vl ) {
    auto ret = static_cast< __dios::ThreadId * >( retval );
    *ret = ctx.scheduler->get_thread_id();
}

} // namespace __sc

namespace __dios {

Thread::Thread( FunPtr fun ) noexcept
    : _frame( static_cast< _VM_Frame * >( __vm_obj_make( fun->frame_size ) ) ),
      _state( State::RUNNING )
{
    _frame->pc = fun->entry_point;
    _frame->parent = nullptr;
}

Thread::Thread( Thread&& o ) noexcept
    : _frame( o._frame ), _state( o._state )
{
    o._frame = 0;
    o._state = State::ZOMBIE;
}

Thread& Thread::operator=( Thread&& o ) noexcept {
    std::swap( _frame, o._frame );
    std::swap( _state, o._state );
    return *this;
}

void Thread::update_state() noexcept {
    if ( !_frame )
        _state = State::ZOMBIE;
}

void Thread::stop() noexcept {
    if ( !active() ) {
        __vm_fault( static_cast< _VM_Fault >( _DiOS_F_Threading ) );
        return;
    }

    clear();
    _state = State::ZOMBIE;
}

void Thread::hard_stop() noexcept {
    _state = State::ZOMBIE;
    clear();
}

void Thread::clear() noexcept {
    while ( _frame ) {
        _VM_Frame *f = _frame->parent;
        __vm_obj_free( _frame );
        _frame = f;
    }
}

Thread *Scheduler::choose_thread() noexcept {
    if ( _cf->thread_count < 1 )
        return nullptr;
    get_active_thread()->update_state();
    int idx = __vm_choose( _cf->thread_count );
    _cf->active_thread = idx;
    Thread &thread = get_threads()[ idx ];
    if ( !thread.zombie() )
        return &thread;
    return nullptr;
}

Thread *Scheduler::choose_live_thread() noexcept {
    if ( _cf->thread_count < 1 )
        return nullptr;
    get_active_thread()->update_state();
    int count = 0;
    for ( int i = 0; i != _cf->thread_count; i++ ) {
        if ( get_threads()[ i ].active() )
            count++;
    }

    if ( count == 0 )
        return nullptr;

    struct PI { int pid, tid, choice; };
    PI *pi = reinterpret_cast< PI * >( __vm_obj_make( count * sizeof( PI ) ) );
    PI *pi_it = pi;
    count = 0;
    for ( int i = 0; i != _cf->thread_count; i++ ) {
        if ( get_threads()[ i ].active() ) {
            pi_it->pid = 1;
            pi_it->tid = i;
            pi_it->choice = count++;
            ++pi_it;
        }
    }

    __vm_trace( _VM_T_SchedChoice, pi );
    int sel = __vm_choose( count );
    auto thr = get_threads();
    while ( sel != 0 )
    {
        if ( thr->active() )
            --sel;
        ++thr;
    }

    while ( !thr->active() )
        ++thr;

    _cf->active_thread = thr - get_threads();
    __dios_assert_v( thr->active(), "thread is inactive" );
    __dios_assert_v( thr->_frame, "frame is invalid" );
    return thr;
}

void Scheduler::start_main_thread( FunPtr main, int argc, char** argv, char** envp ) noexcept {
    __dios_assert_v( main, "Invalid main function!" );

    _DiOS_FunPtr dios_main = __dios_get_fun_ptr( "__dios_main" );
    __dios_assert_v( dios_main, "Invalid DiOS main function" );

    new ( &( _cf->main_thread ) ) Thread( dios_main );
    _cf->active_thread = 0;
    _cf->thread_count = 1;

    DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >(
        _cf->main_thread[ 0 ]._frame );
    frame->l = main->arg_count;

    frame->argc = argc;
    frame->argv = argv;
    frame->envp = envp;
}

ThreadId Scheduler::start_thread( FunPtr routine, void *arg ) noexcept {
    __dios_assert( routine );

    __vm_obj_resize( _cf, __vm_obj_size( _cf ) + sizeof( Thread ) );

    Thread &t = get_threads()[ _cf->thread_count++ ];
    new ( &t ) Thread( routine );
    ThreadRoutineFrame *frame = reinterpret_cast< ThreadRoutineFrame * >( t._frame );
    frame->arg = arg;

    return _cf->thread_count - 1;
}

void Scheduler::kill_thread( ThreadId t_id ) noexcept {
    __dios_assert( int( t_id ) < _cf->thread_count );
    get_threads()[ t_id ].stop();
}

void Scheduler::terminate() noexcept {
    for ( int i = 0; i != _cf->thread_count; i++ ) {
        if ( get_threads()[ i ].active() )
            get_threads()[ i ].hard_stop();
    }
}

} // namespace __dios
