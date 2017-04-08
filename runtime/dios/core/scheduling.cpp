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

namespace __dios {

void sig_ign( int ) {}
void sig_die( int ) {}
void sig_fault( int ) {}

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

const sighandler_t defhandlers[] =
{
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

} // namespace __dios
