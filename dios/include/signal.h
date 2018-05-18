/* $Id$ */

/* 7.14 Signal handling <string.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef _PDCLIB_SIGNAL_H
#define _PDCLIB_SIGNAL_H _PDCLIB_SIGNAL_H
#include <_PDCLIB/config.h>
#include <_PDCLIB/glue.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/* Signals ------------------------------------------------------------------ */

/* A word on signals, to the people using PDCLib in their OS projects.

   The definitions of the C standard leave about everything that *could* be
   useful to be "implementation defined". Without additional, non-standard
   arrangements, it is not possible to turn them into a useful tool.

   This example implementation chose to "not generate any of these signals,
   except as a result of explicit calls to the raise function", which is
   allowed by the standard but of course does nothing for the usefulness of
   <signal.h>.

   A useful signal handling would:
   1) make signal() a system call that registers the signal handler with the OS
   2) make raise() a system call triggering an OS signal to the running process
   3) make provisions that further signals of the same type are blocked until
      the signal handler returns (optional for SIGILL)
*/

/* These are the values used by Linux. */

/* Abnormal termination / abort() */
#define SIGABRT _HOST_SIGABRT
/* Arithmetic exception / division by zero / overflow */
#define SIGFPE  _HOST_SIGFPE
/* Illegal instruction */
#define SIGILL  _HOST_SIGILL
/* Interactive attention signal */
#define SIGINT  _HOST_SIGINT
/* Invalid memory access */
#define SIGSEGV _HOST_SIGSEGV
/* Invalid pipe usage */
#define SIGPIPE _HOST_SIGPIPE
/* Termination request */
#define SIGTERM _HOST_SIGTERM

/* Quit from keyboard */
#define SIGQUIT _HOST_SIGQUIT
/* Bus error (bad memory access) */
#define SIGBUS _HOST_SIGBUS
/* Bad argument to routine */
#define SIGSYS _HOST_SIGSYS
/* Trace/breakpoint trap */
#define SIGTRAP _HOST_SIGTRAP
/* CPU time limit exceeded */
#define SIGXCPU _HOST_SIGXCPU
/* File size limit exceeded */
#define SIGXFSZ _HOST_SIGXFSZ
/* IOT trap. A synonym for SIGABRT */
#define SIGIOT SIGABRT
/* Synonymous with SIGSYS */
#define SIGUNUSED SIGSYS
/* Kill signal */
#define SIGKILL _HOST_SIGKILL
/* Stop process */
#define SIGSTOP _HOST_SIGSTOP
/* Timer expired */
#define SIGALRM _HOST_SIGALRM
/* Signal hang up */
#define SIGHUP _HOST_SIGHUP

/* The following should be defined to pointer values that could NEVER point to
   a valid signal handler function. (They are used as special arguments to
   signal().) Again, these are the values used by Linux.
*/
#define SIG_DFL (void (*)( int ))0
#define SIG_ERR (void (*)( int ))-1
#define SIG_IGN (void (*)( int ))1

#define SA_RESTART 0x10000000  // Linux definition

typedef _PDCLIB_sig_atomic sig_atomic_t;

/* Installs a signal handler "func" for the given signal.
   A signal handler is a function that takes an integer as argument (the signal
   number) and returns void.

   Note that a signal handler can do very little else than:
   1) assign a value to a static object of type "volatile sig_atomic_t",
   2) call signal() with the value of sig equal to the signal received,
   3) call _Exit(),
   4) call abort().
   Virtually everything else is undefind.

   The signal() function returns the previous installed signal handler, which
   at program start may be SIG_DFL or SIG_ILL. (This implementation uses
   SIG_DFL for all handlers.) If the request cannot be honored, SIG_ERR is
   returned and errno is set to an unspecified positive value.
*/
void (*signal( int sig, void (*func)( int ) ) )( int );

/* Raises the given signal (executing the registered signal handler with the
   given signal number as parameter).
   This implementation does not prevent further signals of the same time from
   occuring, but executes signal( sig, SIG_DFL ) before entering the signal
   handler (i.e., a second signal before the signal handler re-registers itself
   or SIG_IGN will end the program).
   Returns zero if successful, nonzero otherwise. */
int raise( int sig ) __nothrow;

int kill(pid_t pid, int sig) __nothrow;

#ifndef _SIGSET_T_DEFINED_
#define _SIGSET_T_DEFINED_
typedef _PDCLIB_uint64_t sigset_t;
#endif

typedef union sigval
{
    int sigval_int;
    void *sigval_ptr;
} sigval_t;

typedef struct siginfo
{
    sigval_t si_action;
} siginfo_t;

struct sigaction
{
    void ( *sa_handler )( int );
    void ( *sa_sigaction )( int, siginfo_t *, void * );
    sigset_t sa_mask;
    int sa_flags;
    void ( *sa_restorer )( void );
};

int sigaction( int signum, const struct sigaction *act, struct sigaction *oldact ) __nothrow;
int rt_sigaction( int signum, const struct sigaction *act, struct sigaction *oldact,
                  size_t sigsetsize ) __nothrow;

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

__END_DECLS

#endif

