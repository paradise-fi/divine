#include <sys/syscall.h>
#include <unistd.h>

pid_t fork( void )
{
    pid_t old_pid = getpid(), child_pid;
    __dios_syscall( SYS_sysfork, &child_pid );
    pid_t new_pid = getpid();
    return ( old_pid == new_pid ) ? child_pid : 0;
}
