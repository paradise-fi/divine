#include <sys/syscall.h>
#include <sys/socket.h>

ssize_t recvfrom( int fd, void *buf, size_t n, int flags,
                  struct sockaddr *addr, socklen_t *addrlen )
{
    ssize_t retval;
    __dios_syscall( SYS_recvfrom ,  &retval, fd, buf, n, flags, addr, addrlen);
    return retval;
}

