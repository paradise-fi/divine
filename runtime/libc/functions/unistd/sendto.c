#include <sys/syscall.h>
#include <sys/socket.h>

ssize_t sendto( int fd, const void *buf, size_t n, int flags,
                const struct sockaddr *addr, socklen_t addrlen )
{
    ssize_t retval;
    __dios_syscall( SYS_sendto,  &retval, fd, buf, n, flags, addr, addrlen );
    return retval;
}
