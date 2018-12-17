#include <sys/syscall.h>
#include <unistd.h>
#include <sys/divm.h>
#include <sys/interrupt.h>
#include <dios.h>

pid_t fork( void ) noexcept
{
    pid_t old_pid = getpid(), child_pid;
    __run_atfork_handlers( 0 );
    __dios::InterruptMask mask;

    __dios_sysfork( &child_pid );
    pid_t new_pid = getpid();
    bool is_parent = old_pid == new_pid;

    mask.without( [=]{ __run_atfork_handlers( is_parent? 1 : 2 ); } );
    return is_parent ? child_pid : 0;
}
