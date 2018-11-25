/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/trace.h>

int main() {
    int fd = open( "file", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    errno = 0;
    assert( mkdir( "file/dir", 0755 ) == -1 );
    __dios_trace_f( "errno = %d", errno );
    int e = errno;
    assert( e == ENOTDIR );

    return 0;
}
