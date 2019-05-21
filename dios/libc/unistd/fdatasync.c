#include <sys/syscall.h>
#include <fcntl.h>

#if !_HOST_is_linux /* linux provides a separate fdatasync as a syscall */

int __libc_fdatasync( int fd )
{
    return __libc_fsync( fd );
}

int fdatasync( int ) __attribute__ ((weak, alias( "__libc_fdatasync" )));

#endif
