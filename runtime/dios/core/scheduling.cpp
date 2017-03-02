// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <dios/core/scheduling.hpp>
#include <dios/core/main.hpp>
#include <divine/metadata.h>
#include <signal.h>
#include <errno.h>

_DiOS_ThreadHandle __dios_start_thread( void ( *routine )( void * ), void *arg, int tls_size ) noexcept
{
    _DiOS_ThreadHandle ret;
    __dios_syscall( __dios::_SC_start_thread, &ret, routine, arg, tls_size );
    return ret;
}

_DiOS_ThreadHandle __dios_get_thread_handle() noexcept {
    return reinterpret_cast< _DiOS_ThreadHandle >
        ( __vm_control( _VM_CA_Get, _VM_CR_User2 ) );
}

int *__dios_get_errno() noexcept {
    return &( __dios_get_thread_handle()->_errno );
}

void __dios_kill_thread( _DiOS_ThreadHandle id ) noexcept {
    __dios_syscall( __dios::_SC_kill_thread, nullptr, id );
}

void __dios_kill_process( pid_t id ) noexcept {
    if ( id == 1 ) // TODO: all kinds of suicide
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
    int ret;
    __dios_syscall( __dios::_SC_kill, &ret, id, SIGKILL );
}

_DiOS_ThreadHandle *__dios_get_process_threads() noexcept {
    _DiOS_ThreadHandle *ret;
    auto tid = __dios_get_thread_handle();
    __dios_syscall( __dios::_SC_get_process_threads, &ret, tid );
    return ret;
}

namespace __sc {

using __dios::sighandler_t;

void start_thread( __dios::Context& ctx, int *, void *retval, va_list vl ) {
    typedef void ( *r_type )( void * );
    auto routine = va_arg( vl, r_type );
    auto arg = va_arg( vl, void * );
    auto tls = va_arg( vl, int );
    auto ret = static_cast< __dios::ThreadHandle * >( retval );

    auto t = ctx.scheduler->newThread( routine, tls );
    ctx.scheduler->setupThread( t, arg );
    __vm_obj_shared( t->getId() );
    __vm_obj_shared( arg );
    *ret = t->getId();
}

static void sig_ign( int ) {}
static void sig_die( int ) {}

static const sighandler_t defhandlers[] = {
        sig_die,    // SIGHUP    = 1
        sig_die,    // SIGINT    = 2
        sig_die,    // SIGQUIT   = 3
        sig_die,    // SIGILL    = 4
        sig_die,    // SIGTRAP   = 5
        sig_die,    // SIGABRT   = 6
        sig_die,    // SIGBUS    = 7
        sig_die,    // SIGFPE    = 8
        sig_die,    // SIGKILL   = 9
        sig_die,    // SIGUSR1   = 10
        sig_die,    // SIGSEGV   = 11
        sig_die,    // SIGUSR2   = 12
        sig_die,    // SIGPIPE   = 13
        sig_die,    // SIGALRM   = 14
        sig_die,    // SIGTERM   = 15
        sig_die,    // SIGSTKFLT = 16
        sig_ign,    // SIGCHLD   = 17
        sig_ign,    // SIGCONT   = 18 ?? this should be OK since it should
        sig_ign,    // SIGSTOP   = 19 ?? stop/resume the whole process, we can
        sig_ign,    // SIGTSTP   = 20 ?? simulate it as doing nothing
        sig_ign,    // SIGTTIN   = 21 ?? at least until we will have processes
        sig_ign,    // SIGTTOU   = 22 ?? and process-aware kill
        sig_ign,    // SIGURG    = 23
        sig_die,    // SIGXCPU   = 24
        sig_die,    // SIGXFSZ   = 25
        sig_die,    // SIGVTALRM = 26
        sig_die,    // SIGPROF   = 27
        sig_ign,    // SIGWINCH  = 28
        sig_die,    // SIGIO     = 29
        sig_die,    // SIGPWR    = 30
        sig_die     // SIGUNUSED = 31

};

void kill( __dios::Context& ctx, int *err, void *ret, va_list vl ) {
    int pid = va_arg( vl, pid_t );
    auto sig = va_arg( vl, int );
    sighandler_t handler;

    bool found = false;
    for ( auto thread : ctx.scheduler->threads )
        if ( thread->_pid == pid )
            found = true;

    if ( !found )
    {
        *err = ESRCH;
        *static_cast< int* >( ret ) = -1;
        return;
    }

    if ( ctx.sighandlers )
        handler = ctx.sighandlers[sig];
    else
        handler = defhandlers[sig];

    if ( handler == sig_ign )
        return;
    if ( handler == sig_die )
        ctx.scheduler->killProcess( pid );
    else
        __dios_fault( _VM_F_NotImplemented, "Custom signal handlers not implemented." );
}

void getpid( __dios::Context& ctx, int *, void *retval, va_list )
{
    auto tid = __dios_get_thread_handle();
    auto thread = ctx.scheduler->threads.find( tid );
    __dios_assert( thread );
    *static_cast< int * >( retval ) = thread->_pid;
}


void kill_thread( __dios::Context& ctx, int *, void *, va_list vl ) {
    auto id = va_arg( vl, __dios::ThreadHandle );
    ctx.scheduler->killThread( id );
}

void kill_process( __dios::Context& ctx, int *, void *, va_list vl ) {
    auto id = va_arg( vl, pid_t );
    ctx.scheduler->killProcess( id );
}

void get_process_threads( __dios::Context &ctx, int *, void *_ret, va_list vl ) {
    auto *&ret = *reinterpret_cast< _DiOS_ThreadHandle ** >( _ret );
    auto tid = va_arg( vl, _DiOS_ThreadHandle );
    pid_t pid;
    for ( auto &t : ctx.scheduler->threads ) {
        if ( t->_tls == tid ) {
            pid = t->_pid;
            break;
        }
    }
    int count = 0;
    for ( auto &t : ctx.scheduler->threads ) {
        if ( t->_pid == pid )
            ++count;
    }
    ret = static_cast< _DiOS_ThreadHandle * >( __vm_obj_make( sizeof( _DiOS_ThreadHandle ) * count ) );
    int i = 0;
    for ( auto &t : ctx.scheduler->threads ) {
        if ( t->_pid == pid ) {
            ret[ i ] = t->_tls;
            ++i;
        }
    }
}


} // namespace __sc

namespace __sc_passthru {

void start_thread( __dios::Context& ctx, int * err, void* retval, va_list vl )  {
    __sc::start_thread(ctx, err, retval, vl);
}

void kill_thread( __dios::Context& ctx, int * err, void* retval, va_list vl ) {
    __sc::kill_thread(ctx, err, retval, vl);
}

void kill_process( __dios::Context& ctx, int * err, void* retval, va_list vl ) {
    __sc::kill_process(ctx, err, retval, vl);
}

void get_process_threads( __dios::Context& ctx, int * err, void* retval, va_list vl ) {
    __sc::get_process_threads(ctx, err, retval, vl);
}
} // namespace __sc_passthru

namespace __dios {

Thread::Thread( Thread&& o ) noexcept
    : _frame( o._frame ), _tls( o._tls ), _pid( o._pid )
{
    o._frame = nullptr;
    o._tls = nullptr;
    o._pid = -1;
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
    int c = threads.size();
    if ( c == 0 )
        return;
    struct PI { unsigned pid, tid, choice; };
    PI *pi = reinterpret_cast< PI * >( __vm_obj_make( c * sizeof( PI ) ) );
    PI *pi_it = pi;
    for ( int i = 0; i != c; i++ ) {
        pi_it->pid = 0;
        pi_it->tid = threads[ i ]->getUserId();
        pi_it->choice = i;
        ++pi_it;
    }

    __vm_trace( _VM_T_SchedChoice, pi );
    __vm_obj_free( pi );
}

void Scheduler::setupMainThread( Thread *t, int argc, char** argv, char** envp ) noexcept
{
    DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >( t->_frame );
    frame->l = __md_get_pc_meta( reinterpret_cast< uintptr_t >( main ) )->arg_count;

    frame->argc = argc;
    frame->argv = argv;
    frame->envp = envp;
}

void Scheduler::setupThread( Thread *t, void *arg ) noexcept
{
    ThreadRoutineFrame *frame = reinterpret_cast< ThreadRoutineFrame * >( t->_frame );
    frame->arg = arg;
}

void Scheduler::killThread( ThreadHandle tid ) noexcept {
    bool res = threads.remove( tid );
    __dios_assert_v( res, "Killing non-existing thread" );
}

void Scheduler::killProcess( pid_t id ) noexcept {
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
