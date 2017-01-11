#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <divine.h>
#include <dios/syscall.hpp>
#include <cerrno>

int open( const char *path, int flags, ... )
{
	int mode = 0;
	if ( flags & O_CREAT ) {
		va_list vl;
		va_start(vl, flags);
		mode = va_arg(vl, int);
		va_end(vl);
	}

	int ret;
    __dios_syscall( __dios::_SC_open, &ret, path, flags, mode );

    return ret;
}

int openat( int dirfd, const char *path, int flags, ... )
{
	int mode = 0;
	if ( flags & O_CREAT ) {
		va_list vl;
		va_start(vl, flags);
		mode = va_arg(vl, int);
		va_end(vl);
	}

	int ret;
    __dios_syscall( __dios::_SC_openat, &ret, dirfd, path, flags, mode );

    return ret;
}

int fcntl( int fd, int cmd, ... )
{
	int ret;
	if ((cmd & F_DUPFD) | (cmd & F_DUPFD) | (cmd & F_SETFD) | (cmd & F_SETFL) |
		(cmd & F_DUPFD_CLOEXEC) | (cmd & F_SETOWN))
	{
		va_list vl;
		va_start(vl, cmd);
		auto arq = va_arg(vl, int);
		va_end(vl);
		__dios_syscall( __dios::_SC_openat, &ret, fd, cmd, arq );
	} else if ( (cmd & F_SETLK) | (cmd & F_SETLKW) | (cmd & F_GETLK)  ) {
		va_list vl;
		va_start(vl, cmd);
		auto arq = va_arg(vl, struct flock *);
		va_end(vl);
		__dios_syscall( __dios::_SC_openat, &ret, fd, cmd, arq );
	} else {
		__dios_syscall( __dios::_SC_openat, &ret, fd, cmd );
	}

	return ret;
}

int pipe( int pipefd[ 2 ] )
{
	int ret;
	__dios_syscall( __dios::_SC_pipe, &ret, pipefd );

	return ret;
}

void sync( void )
{
	__dios_syscall( __dios::_SC_pipe, nullptr );

}


ssize_t recvfrom( int fd, void *buf, size_t n, int flags,
                         struct sockaddr *addr, socklen_t *addrlen )
{
	ssize_t retval;
	__dios_syscall( __dios::_SC_recvfrom ,  &retval, fd, buf, n, flags, addr, addrlen);
	return retval;
}

int socketpair( int domain, int type, int protocol,int fds[2] ){

	int retval;
	__dios_syscall( __dios::_SC_socketpair,  &retval, domain, type, protocol, fds);
	return retval;
}

ssize_t sendto( int fd, const void *buf, size_t n, int flags,
	const struct sockaddr *addr, socklen_t addrlen )
{
	ssize_t retval;
	__dios_syscall( __dios::_SC_sendto,  &retval, fd, buf, n, flags, addr, addrlen );
	return retval;
}


#define SYSCALL0(name, sched, ret) ret name( ) { \
	ret retval;\
	 __dios_syscall( __dios::_SC_ ## name  &retval,);\
    return retval; }
#define SYSCALL1(name, sched, ret, arg1, name1) ret name( arg1 name1) { \
	ret retval;\
	 __dios_syscall( __dios::_SC_ ## name, &retval, name1);\
    return retval; }
#define SYSCALL2(name, sched, ret, arg1, name1, arg2, name2) ret name( arg1 name1, arg2 name2) { \
	ret retval;\
	 __dios_syscall( __dios::_SC_ ## name, &retval, name1, name2 );\
    return retval; }
#define SYSCALL3(name, sched, ret, arg1, name1, arg2, name2, arg3, name3) ret name( arg1 name1, arg2 name2,  arg3 name3) { \
	ret retval;\
	 __dios_syscall( __dios::_SC_ ## name, &retval, name1, name2, name3 );\
    return retval; }
#define SYSCALL4(name, sched, ret, arg1, name1, arg2, name2,  arg3, name3,  arg4, name4) \
 ret name( arg1 name1, arg2 name2, arg3 name3,  arg4 name4) { \
	ret retval;\
	 __dios_syscall( __dios::_SC_ ## name, &retval, name1, name2, name3, name4 );\
    return retval; }
#define SYSCALL5(name, sched, ret, arg1, name1, arg2, name2,  arg3, name3,  arg4, name4, arg5, name5) \
    ret name( arg1 name1, arg2 name2, arg3 name3,  arg4 name4, arg5 name5) { \
	ret retval;\
	 __dios_syscall( __dios::_SC_ ## name, &retval, name1, name2, name3, name4, name5 );\
    return retval; }
#define SYSCALLSEP(...)
    #include <dios/syscall.def>
