#include <sys/syscall.h>

int pipe( int pipefd[ 2 ] )
{
    return __dios_pipe( pipefd );
}
