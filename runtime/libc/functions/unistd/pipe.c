#include <sys/syscall.h>

int pipe( int pipefd[ 2 ] )
{
	int ret;
	__dios_syscall( SYS_pipe, &ret, pipefd );

	return ret;
}
