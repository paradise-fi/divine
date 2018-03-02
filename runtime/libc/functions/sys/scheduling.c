#include <dios.h>
#include <signal.h>
#include <sys/syscall.h>

__dios_task *__dios_get_process_tasks()
{
    __dios_task *ret;
    __dios_syscall( SYS_get_process_tasks, &ret, __dios_this_task() );
    return ret;
}

int __dios_hardware_concurrency()
{
    int ret;
    __dios_syscall( SYS_hardware_concurrency, &ret );
    return ret;
}

__dios_task __dios_start_task( __dios_task_routine routine, void *arg, int tls_size )
{
    __dios_task ret;
    __dios_syscall( SYS_start_task, &ret, routine, arg, tls_size );
    return ret;
}

void __dios_die() { __dios_syscall( SYS_die, NULL ); }
void __dios_yield() { __dios_syscall( SYS_yield, NULL ); }
void __dios_kill( __dios_task id ) { __dios_syscall( SYS_kill_task, NULL, id ); }
