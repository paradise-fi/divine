#include <sys/wait.h>

pid_t wait(int *wstatus)
{
    return waitpid( -1, wstatus, 0 );
}

pid_t waitpid( pid_t pid, int *wstatus, int options )
{
    return wait4( pid, wstatus, options, 0 );
}

pid_t wait3( int *wstatus, int options, struct rusage *rusage )
{
    return wait4( -1, wstatus, options, rusage );
}
