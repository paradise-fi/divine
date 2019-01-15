#include <sys/hostabi.h>
#include <signal.h>
#include <sys/syswrap.h>

#if _HOST_is_linux /* linux only provides rt_sigaction */

int __libc_sigaction( int signum, const struct sigaction *act, struct sigaction *old )
{
    return __libc_rt_sigaction( signum, act, old, sizeof( sigset_t ) );
}

int sigaction( int, const struct sigaction *, struct sigaction * ) __attribute__ ((weak, alias ("__libc_sigaction")));

#endif
