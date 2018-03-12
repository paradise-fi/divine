/* TAGS: c min */

#include <sys/syscall.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

int main()
{
    int fd = syscall( SYS_open, "/test", O_WRONLY | O_CREAT, 0666 );
    syscall( SYS_write, fd, "hello", 5ul );
    syscall( SYS_close, fd );

    char buf[6] = { 0 };
    fd = open( "/test", O_RDONLY );
    read( fd, buf, 5 );
    assert( !strcmp( buf, "hello" ) );
}
