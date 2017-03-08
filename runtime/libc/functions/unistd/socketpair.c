#include <sys/syscall.h>

int socketpair( int domain, int type, int protocol, int fds[2] )
{
    int retval;
    __dios_syscall( SYS_socketpair,  &retval, domain, type, protocol, fds );
    return retval;
}
