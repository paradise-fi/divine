// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <dios/scheduling.hpp>
#include <dios/main.hpp>
#include <divine/metadata.h>

_DiOS_ThreadId __dios_start_thread( void ( *routine )( void * ), void *arg, size_t tls_size ) noexcept
{
    _DiOS_ThreadId ret;
    __dios_syscall( __dios::_SC_START_THREAD, &ret, routine, arg, tls_size );
    return ret;
}

_DiOS_ThreadId __dios_get_thread_id() noexcept {
    return __vm_control( _VM_CA_Get, _VM_CR_User2 );
}

void __dios_kill_thread( _DiOS_ThreadId id ) noexcept {
    __dios_syscall( __dios::_SC_KILL_THREAD, nullptr, id );
}

void __dios_kill_process( _DiOS_ProcId id ) noexcept {
    __dios_syscall( __dios::_SC_KILL_PROCESS, nullptr, id );
}

namespace __sc {

void start_thread( __dios::Context& ctx, void *retval, va_list vl ) {
    typedef void ( *r_type )( void * );
    auto routine = va_arg( vl, r_type );
    auto arg = va_arg( vl, void * );
    auto tls = va_arg( vl, size_t );
    auto ret = static_cast< __dios::ThreadId * >( retval );

    *ret = ctx.scheduler->startThread( routine, arg, tls );
}

void kill_thread( __dios::Context& ctx, void *, va_list vl ) {
    auto id = va_arg( vl, __dios::ThreadId );
    ctx.scheduler->killThread( id );
}

void kill_process( __dios::Context& ctx, void *, va_list vl ) {
    auto id = va_arg( vl, __dios::ProcId );
    ctx.scheduler->killProcess( id );
}

} // namespace __sc

namespace __dios {

Thread::Thread( Thread&& o ) noexcept
    : _frame( o._frame ), _tls( o._tls ), _pid( o._pid )
{
    o._frame = nullptr;
    o._tls = nullptr;
    o._pid = nullptr;
}

Thread& Thread::operator=( Thread&& o ) noexcept {
    std::swap( _frame, o._frame );
    std::swap( _tls, o._tls );
    std::swap( _pid, o._pid);
    return *this;
}

Thread::~Thread() noexcept {
    clearFrame();
    __vm_obj_free( _tls );
}

void Thread::clearFrame() noexcept {
    while ( _frame ) {
        _VM_Frame *f = _frame->parent;
        __vm_obj_free( _frame );
        _frame = f;
    }
}

Thread *Scheduler::chooseThread() noexcept
{
    if ( threads.empty() )
        return nullptr;
    return threads[ __vm_choose( threads.size() ) ];
}

void Scheduler::traceThreads() const noexcept {
    size_t c = threads.size();
    if ( c == 0 )
        return;
    struct PI { int pid, tid, choice; };
    PI *pi = reinterpret_cast< PI * >( __vm_obj_make( c * sizeof( PI ) ) );
    PI *pi_it = pi;
    for ( int i = 0; i != c; i++ ) {
        pi_it->pid = 0;
        pi_it->tid = threads[ i ]->getUserId();
        pi_it->choice = i;
        ++pi_it;
    }

    __vm_trace( _VM_T_SchedChoice, pi );
}

void Scheduler::startMainThread( int argc, char** argv, char** envp ) noexcept {
    Thread *t = threads.emplace( _start, 0 );
    DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >( t->_frame );
    frame->l = __md_get_pc_meta( reinterpret_cast< uintptr_t >( main ) )->arg_count;

    frame->argc = argc;
    frame->argv = argv;
    frame->envp = envp;
}

ThreadId Scheduler::startThread( void ( *routine )( void * ), void *arg, size_t tls_size ) noexcept {
    __dios_assert_v( routine, "Invalid thread routine" );
    Thread *t = threads.emplace( routine, tls_size );
    ThreadRoutineFrame *frame = reinterpret_cast< ThreadRoutineFrame * >( t->_frame );
    frame->arg = arg;

    return t->getId();
}

void Scheduler::killThread( ThreadId tid ) noexcept {
    bool res = threads.remove( tid );
    __dios_assert_v( res, "Killing non-existing thread" );
}

void Scheduler::killProcess( ProcId id ) noexcept {
    if ( !id ) {
        threads.erase( threads.begin(), threads.end() );
        // ToDo: Erase processes
        return;
    }

    auto r = std::remove_if( threads.begin(), threads.end(), [&]( Thread *t ) {
        return t->_pid == id;
    });
    threads.erase( r, threads.end() );
    // ToDo: Erase processes

}

} // namespace __dios
