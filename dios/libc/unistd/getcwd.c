#include <sys/hostabi.h>

#if _HOST_is_openbsd /* on OpenBSD, the real syscall is __getcwd */

#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

char *__libc_getcwd( char *buf, size_t size )
{
    char *allocated = NULL;

    if ( buf != NULL && size == 0 )
    {
        errno = EINVAL;
        return NULL;
    }

    if ( !buf && !( allocated = buf = malloc( size = PATH_MAX ) ) )
        return NULL;

    if ( __getcwd(buf, size) == -1 )
    {
        free( allocated );
        return NULL;
    }

    return buf;
}

char *getcwd( char *, size_t ) __attribute__ ((weak, alias( "__libc_getcwd" )));

#endif
