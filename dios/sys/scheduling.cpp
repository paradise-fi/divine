// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>
//                 2016 Petr Rockai <me@mornfall.net>

#include <sys/signal.h>
#include <signal.h>
#include <errno.h>
#include <sys/lart.h>

#include <dios/sys/scheduling.hpp>

namespace __dios {

void sig_ign( int ) {}
void sig_die( int ) {}
void sig_fault( int ) {}

const sighandler_t defhandlers[] =
{
    { sig_ign, 0 },  // fluff, so that signal numbers match
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

extern "C" __trapfn __invisible __weakmem_direct void __dios_interrupt()
{
    uint64_t flags = uint64_t( __vm_ctl_get( _VM_CR_Flags ) );

    if ( flags & _VM_CF_KernelMode )
        __dios_fault( _VM_F_Control, "oops, interrupted in kernel mode" );

    if ( flags & _DiOS_CF_Mask )
    {
        __vm_ctl_flag( 0, _DiOS_CF_Deferred );
        return;
    }

    __vm_ctl_flag( _DiOS_CF_Deferred, 0 );
    __dios_sync_parent_frame();
    __vm_suspend();
}

} // namespace __dios
