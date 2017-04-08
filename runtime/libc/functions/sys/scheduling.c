#include <dios.h>
#include <signal.h>
#include <sys/syscall.h>

_DiOS_ThreadHandle *__dios_get_process_threads() {
    _DiOS_ThreadHandle *ret;
    __dios_syscall( SYS_get_process_threads, &ret, __dios_get_thread_handle() );
    return ret;
}

int __dios_hardware_concurrency() {
    int ret;
    __dios_syscall( SYS_hardware_concurrency, &ret );
    return ret;
}

void __dios_kill_thread( _DiOS_ThreadHandle id ) {
    __dios_syscall( SYS_kill_thread, NULL, id );
}

void __dios_kill_process( pid_t id ) {
    if ( id == 1 ) // TODO: all kinds of suicide
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
    __dios_syscall( SYS_kill, NULL, id, SIGKILL );
}

_DiOS_ThreadHandle __dios_start_thread( void ( *routine )( void * ), void *arg, int tls_size ) {
    _DiOS_ThreadHandle ret;
    __dios_syscall( SYS_start_thread, &ret, routine, arg, tls_size );
    return ret;
}

void __dios_die() {
    __dios_syscall( SYS_die, NULL );
}
