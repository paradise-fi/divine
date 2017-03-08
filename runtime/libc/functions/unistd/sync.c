#include <sys/syscall.h>

void sync( void )
{
    __dios_syscall( SYS_pipe, (void *) 0 );
}
