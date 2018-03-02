#include <sys/hostabi.h>
#include <signal.h>

#if _HOST_is_linux /* linux only provides rt_sigaction */

int sigaction( int signum, const struct sigaction *act, struct sigaction *old )
{
    return rt_sigaction( signum, act, old, sizeof( sigset_t ) );
}

#endif
