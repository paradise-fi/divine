#include <sys/syscall.h>
#include <unistd.h>
#include <sys/divm.h>
#include <sys/interrupt.h>
#include <dios.h>

pid_t fork( void )
{
    pid_t old_pid = getpid(), child_pid;
    __run_atfork_handlers( 0 );
    __dios::InterruptMask mask;

    auto *tasks = __dios_get_process_tasks();
    __dios_syscall( SYS_sysfork, &child_pid );
    pid_t new_pid = getpid();
    bool is_parent = old_pid == new_pid;

    if ( !is_parent )
    {
        auto current_task = __dios_this_task();
        int cnt = __vm_obj_size( tasks ) / sizeof( __dios_task );
        for ( int i = 0; i < cnt; ++i )
            if ( tasks[ i ] != current_task )
                 __vm_obj_free( tasks[ i ] );
    }

    mask.without( [=]{ __run_atfork_handlers( is_parent? 1 : 2 ); } );
    return is_parent ? child_pid : 0;
}
