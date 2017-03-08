#include <sys/syscall.h>
#include <fcntl.h>

int fcntl( int fd, int cmd, ... )
{
    int ret;
    if ( (cmd & F_DUPFD) | (cmd & F_DUPFD) | (cmd & F_SETFD) | (cmd & F_SETFL) |
         (cmd & F_DUPFD_CLOEXEC) | (cmd & F_SETOWN) )
    {
        va_list vl;
        va_start(vl, cmd);
        int arq = va_arg(vl, int);
        va_end(vl);
        __dios_syscall( SYS_openat, &ret, fd, cmd, arq );
    } else if ( (cmd & F_SETLK) | (cmd & F_SETLKW) | (cmd & F_GETLK) )
    {
        va_list vl;
        va_start(vl, cmd);
        struct flock *arq = va_arg(vl, struct flock *);
        va_end(vl);
        __dios_syscall( SYS_openat, &ret, fd, cmd, arq );
    } else
        __dios_syscall( SYS_openat, &ret, fd, cmd );

    return ret;
}
