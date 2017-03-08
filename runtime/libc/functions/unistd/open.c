#include <sys/syscall.h>
#include <fcntl.h>

int open( const char *path, int flags, ... )
{
    int mode = 0;

    if ( flags & O_CREAT )
    {
        va_list vl;
        va_start(vl, flags);
        mode = va_arg(vl, int);
        va_end(vl);
    }

    int ret;
    __dios_syscall( SYS_open, &ret, path, flags, mode );

    return ret;
}
