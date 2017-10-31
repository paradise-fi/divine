#include <dios.h>
#include <signal.h>
#include <sys/syscall.h>

_DiOS_TaskHandle *__dios_get_process_tasks() {
    _DiOS_TaskHandle *ret;
    __dios_syscall( SYS_get_process_tasks, &ret, __dios_get_task_handle() );
    return ret;
}

int __dios_hardware_concurrency() {
    int ret;
    __dios_syscall( SYS_hardware_concurrency, &ret );
    return ret;
}

void __dios_kill_task( _DiOS_TaskHandle id ) {
    __dios_syscall( SYS_kill_task, NULL, id );
}

_DiOS_TaskHandle __dios_start_task( void ( *routine )( void * ), void *arg, int tls_size ) {
    _DiOS_TaskHandle ret;
    __dios_syscall( SYS_start_task, &ret, routine, arg, tls_size );
    return ret;
}

void __dios_die() {
    __dios_syscall( SYS_die, NULL );
}

void __dios_yield() {
    __dios_syscall( SYS_yield, NULL );
}