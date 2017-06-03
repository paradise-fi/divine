#include <unistd.h>

pid_t fork( void )
{
    pid_t old_pid = getpid();
    pid_t child_pid = sysfork();
    pid_t new_pid = getpid();
    return ( old_pid == new_pid ) ? child_pid : 0;
}
