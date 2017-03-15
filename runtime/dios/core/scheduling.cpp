// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <dios/core/scheduling.hpp>
#include <dios/core/main.hpp>
#include <divine/metadata.h>
#include <sys/signal.h>
#include <signal.h>
#include <errno.h>

_DiOS_ThreadHandle __dios_start_thread( void ( *routine )( void * ), void *arg, int tls_size ) noexcept
{
    _DiOS_ThreadHandle ret;
    __dios_syscall( SYS_start_thread, &ret, routine, arg, tls_size );
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
    __dios_syscall( SYS_kill_thread, nullptr, id );
}

void __dios_kill_process( pid_t id ) noexcept {
    if ( id == 1 ) // TODO: all kinds of suicide
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
    int ret;
    __dios_syscall( SYS_kill, &ret, id, SIGKILL );
}

_DiOS_ThreadHandle *__dios_get_process_threads() noexcept {
    _DiOS_ThreadHandle *ret;
    auto tid = __dios_get_thread_handle();
    __dios_syscall( SYS_get_process_threads, &ret, tid );
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
static void sig_fault( int ) {}

static const sighandler_t defhandlers[] = {
    { sig_die, 0 },    // SIGHUP    = 1
    { sig_die, 0 },    // SIGINT    = 2
    { sig_fault, 0 },  // SIGQUIT   = 3
    { sig_fault, 0 },  // SIGILL    = 4
    { sig_fault, 0 },  // SIGTRAP   = 5
    { sig_fault, 0 },  // SIGABRT/SIGIOT   = 6
    { sig_fault, 0 },  // SIGBUS    = 7
    { sig_fault, 0 },  // SIGFPE    = 8
    { sig_die, 0 },    // SIGKILL   = 9
    { sig_die, 0 },    // SIGUSR1   = 10
    { sig_fault, 0 },  // SIGSEGV   = 11
    { sig_die, 0 },    // SIGUSR2   = 12
    { sig_die, 0 },    // SIGPIPE   = 13
    { sig_die, 0 },    // SIGALRM   = 14
    { sig_die, 0 },    // SIGTERM   = 15
    { sig_die, 0 },    // SIGSTKFLT = 16
    { sig_ign, 0 },    // SIGCHLD   = 17
    { sig_ign, 0 },    // SIGCONT   = 18 ?? this should be OK since it should
    { sig_ign, 0 },    // SIGSTOP   = 19 ?? stop/resume the whole process, we can
    { sig_ign, 0 },    // SIGTSTP   = 20 ?? simulate it as doing nothing
    { sig_ign, 0 },    // SIGTTIN   = 21 ?? at least until we will have processes
    { sig_ign, 0 },    // SIGTTOU   = 22 ?? and process-aware kill
    { sig_ign, 0 },    // SIGURG    = 23
    { sig_fault, 0 },  // SIGXCPU   = 24
    { sig_fault, 0 },  // SIGXFSZ   = 25
    { sig_die, 0 },    // SIGVTALRM = 26
    { sig_die, 0 },    // SIGPROF   = 27
    { sig_ign, 0 },    // SIGWINCH  = 28
    { sig_die, 0 },    // SIGIO     = 29
    { sig_die, 0 },    // SIGPWR    = 30
    { sig_fault, 0 }   // SIGUNUSED/SIGSYS = 31
};

void kill( __dios::Context& ctx, int *err, void *ret, va_list vl ) {
    int pid = va_arg( vl, pid_t );
    auto sig = va_arg( vl, int );
    sighandler_t handler;

    bool found = false;
    __dios::Thread *thread;
    for ( auto t : ctx.scheduler->threads )
        if ( t->_pid == pid )
        {
            found = true;
            thread = t;
            break;
        }

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

    *static_cast< int* >( ret ) = 0;

    if ( handler.f == sig_ign )
        return;
    if ( handler.f == sig_die )
        ctx.scheduler->killProcess( pid );
    else if ( handler.f == sig_fault )
        __dios_fault( _VM_F_Control, "Uncaught signal." );
    else
    {
        auto fun = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( __dios_signal_trampoline ) );
        auto _frame = static_cast< __dios::TrampolineFrame * >( __vm_obj_make( fun->frame_size ) );
        _frame->pc = reinterpret_cast< _VM_CodePointer >( __dios_signal_trampoline );
        _frame->interrupted = thread->_frame;
        thread->_frame = _frame;
        _frame->parent = nullptr;
        _frame->handler = handler.f;
    }
}


void sigaction( __dios::Context& ctx, int *err, void *ret, va_list vl )
{
    int sig = va_arg( vl, int );
    auto act = va_arg( vl, const struct sigaction * );
    auto oldact = va_arg( vl, struct sigaction * );

    if ( sig < 0 || sig > static_cast<int>( sizeof(__sc::defhandlers) / sizeof(__dios::sighandler_t) )
        || sig == SIGKILL || sig == SIGSTOP )
    {
        *static_cast< int * >( ret ) = -1;
        *err = EINVAL;
        return;
    }

    if ( !ctx.sighandlers )
    {
        ctx.sighandlers = reinterpret_cast< sighandler_t * >(
                __vm_obj_make( sizeof( defhandlers ) ) );
        std::memcpy( ctx.sighandlers, defhandlers, sizeof(defhandlers) );
    }

    oldact->sa_handler = ctx.sighandlers[sig].f;
    ctx.sighandlers[sig].f = act->sa_handler;
    *static_cast< int * >( ret ) = 0;
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
    frame->l = __md_get_pc_meta( reinterpret_cast< _VM_CodePointer >( main ) )->arg_count;

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
