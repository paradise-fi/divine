#include <sys/syscall.h>
#include <unistd.h>

void _run_atfork_handlers( unsigned short index );

pid_t fork( void )
{
    pid_t old_pid = getpid(), child_pid;
    _run_atfork_handlers( 0 );
    __dios_syscall( SYS_sysfork, &child_pid );
    pid_t new_pid = getpid();
    if ( old_pid == new_pid )
    {
        _run_atfork_handlers( 1 );
        return child_pid;
    }
    else
    {
        _run_atfork_handlers( 2 );
        return 0;
    }
}
