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

    auto *threads = __dios_get_process_threads();
    __dios_syscall( SYS_sysfork, &child_pid );
    pid_t new_pid = getpid();
    bool is_parent = old_pid == new_pid;

    if ( !is_parent )
    {
        auto current_thread = __dios_get_thread_handle();
        int cnt = __vm_obj_size( threads ) / sizeof( _DiOS_ThreadHandle );
        for ( int i = 0; i < cnt; ++i )
            if ( threads[ i ] != current_thread )
                 __vm_obj_free( threads[ i ] );
}

    mask.without( [=]{ __run_atfork_handlers( is_parent? 1 : 2 ); } );
    return is_parent ? child_pid : 0;
}
