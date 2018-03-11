#include <sys/syscall.h>
#include <fcntl.h>

int fcntl( int fd, int cmd, ... )
{
    va_list ap;
    va_start( ap, cmd );
    return __dios_fcntl( fd, cmd, &ap );
}
